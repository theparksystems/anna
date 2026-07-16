#pragma once

#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Model/ProjectModel.h"
#include "../Model/SampleManager.h"

namespace sampr
{

class SampleImportService
{
public:
    struct ImportResult
    {
        int loadedCount = 0;
        int failedCount = 0;
        juce::String lastError;
        std::vector<AssetId> loadedAssetIds;
    };

    static juce::String getSupportedFileChooserPatterns();
    static bool isSupportedAudioFile (const juce::File& file);
    static bool canImportPath (const juce::File& file);
    static juce::StringArray collectImportableFiles (const juce::StringArray& paths);

    static ImportResult importFiles (const juce::StringArray& paths,
                                     ProjectModel& project,
                                     SampleManager& samples,
                                     juce::AudioFormatManager& formats);
};

} // namespace sampr
