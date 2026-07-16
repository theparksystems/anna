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

bool SampleImportService::canImportPath (const juce::File& file)
{
    if (isSupportedAudioFile (file))
        return true;

    if (! file.isDirectory())
        return false;

    juce::DirectoryIterator iterator (file, true);
    while (iterator.next())
        if (isSupportedAudioFile (iterator.getFile()))
            return true;

    return false;
}

juce::StringArray SampleImportService::collectImportableFiles (const juce::StringArray& paths)
{
    juce::StringArray files;

    for (const auto& path : paths)
    {
        const juce::File file (path);

        if (isSupportedAudioFile (file))
        {
            files.add (file.getFullPathName());
            continue;
        }

        if (! file.isDirectory())
            continue;

        juce::DirectoryIterator iterator (file, true);
        while (iterator.next())
        {
            const auto candidate = iterator.getFile();
            if (isSupportedAudioFile (candidate))
                files.add (candidate.getFullPathName());
        }
    }

    files.removeDuplicates (false);
    return files;
}

SampleImportService::ImportResult SampleImportService::importFiles (
    const juce::StringArray& paths,
    ProjectModel& project,
    SampleManager& samples,
    juce::AudioFormatManager& formats)
{
    ImportResult result;

    const auto importableFiles = collectImportableFiles (paths);

    for (const auto& path : importableFiles)
    {
        const juce::File file (path);

        const auto assetId = samples.loadFromFile (file, formats);

        if (! assetId.has_value())
        {
            ++result.failedCount;
            result.lastError = "Could not decode: " + file.getFullPathName();
            continue;
        }

        if (auto* asset = samples.getAsset (*assetId); asset != nullptr && asset->refId.isEmpty())
            asset->refId = project.allocateSampleRefId();

        result.loadedAssetIds.push_back (*assetId);
        ++result.loadedCount;
    }

    return result;
}

} // namespace sampr
