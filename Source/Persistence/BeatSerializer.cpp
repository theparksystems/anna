#include "BeatSerializer.h"

#include "ProjectSerializer.h"

namespace sampr
{

bool BeatSerializer::saveToFile (const Pattern& pattern, const juce::File& beatFile, juce::String& errorOut)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", kFormatId);
    root->setProperty ("version", kFormatVersion);
    root->setProperty ("pattern", ProjectSerializer::serializePattern (pattern));

    const auto json = juce::JSON::toString (juce::var (root), true);

    if (json.isEmpty())
    {
        errorOut = "Failed to serialize beat.";
        return false;
    }

    beatFile.getParentDirectory().createDirectory();

    if (! beatFile.replaceWithText (json))
    {
        errorOut = "Could not write beat file.";
        return false;
    }

    return true;
}

BeatLoadResult BeatSerializer::loadFromFile (const juce::File& beatFile)
{
    BeatLoadResult result;

    if (! beatFile.existsAsFile())
    {
        result.errorMessage = "Beat file does not exist.";
        return result;
    }

    juce::var parsed;
    const auto parseResult = juce::JSON::parse (beatFile.loadFileAsString(), parsed);

    if (parseResult.failed())
    {
        result.errorMessage = parseResult.getErrorMessage();
        return result;
    }

    if (! parsed.isObject())
    {
        result.errorMessage = "Beat file is not a valid JSON object.";
        return result;
    }

    const auto format = parsed.getProperty ("format", {}).toString();
    if (format != kFormatId && format != kLegacyFormatId)
    {
        result.errorMessage = "Unrecognized beat file format.";
        return result;
    }

    const auto version = static_cast<int> (parsed.getProperty ("version", 0));

    if (version > kFormatVersion)
    {
        result.errorMessage = "Beat file version is newer than this build.";
        return result;
    }

    const auto patternVar = parsed.getProperty ("pattern", juce::var());

    if (! ProjectSerializer::deserializePattern (patternVar, result.pattern))
    {
        result.errorMessage = "Beat file is missing valid pattern data.";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace sampr
