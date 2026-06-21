#pragma once

#include <juce_core/juce_core.h>

#include "../Model/Pattern.h"
#include "../Model/ProjectModel.h"

namespace sampr
{

struct ProjectLoadResult
{
    bool success = false;
    juce::String errorMessage;
    ProjectModel model;
    juce::File projectFile;
};

class ProjectSerializer
{
public:
    static constexpr const char* kFormatId = "sampr-project";
    static constexpr int kFormatVersion = 1;

    static bool saveToFile (const ProjectModel& model,
                            const juce::File& projectFile,
                            juce::String& errorOut);

    static ProjectLoadResult loadFromFile (const juce::File& projectFile);

    static juce::var toVar (const ProjectModel& model, const juce::File& projectRoot);
    static bool fromVar (const juce::var& root, ProjectModel& modelOut, juce::String& errorOut);

    static juce::var serializePattern (const Pattern& pattern);
    static bool deserializePattern (const juce::var& root, Pattern& patternOut);
};

} // namespace sampr