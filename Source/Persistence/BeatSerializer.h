#pragma once

#include <juce_core/juce_core.h>

#include "../Model/Pattern.h"

namespace sampr
{

struct BeatLoadResult
{
    bool success = false;
    juce::String errorMessage;
    Pattern pattern;
};

class BeatSerializer
{
public:
    static constexpr const char* kFormatId = "anna-beat";
    static constexpr const char* kLegacyFormatId = "sampr-beat";
    static constexpr int kFormatVersion = 1;

    static bool saveToFile (const Pattern& pattern, const juce::File& beatFile, juce::String& errorOut);
    static BeatLoadResult loadFromFile (const juce::File& beatFile);
};

} // namespace sampr
