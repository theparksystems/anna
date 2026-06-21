#pragma once

#include <juce_core/juce_core.h>

#include "../Model/Pattern.h"

namespace sampr
{

class ProjectModel;
struct SampleAsset;
class LevelMeterBank;

enum class ContextScope
{
    project,
    pattern,
    channel,
    slice
};

struct TrackContextInput
{
    ContextScope scope = ContextScope::channel;
    const ProjectModel* project = nullptr;
    const Pattern* pattern = nullptr;
    int patternIndex = 0;
    int channelIndex = 0;
    const SampleAsset* sampleAsset = nullptr;
    int sliceIndex = 0;
    const LevelMeterBank* meters = nullptr;
    double bpm = 120.0;
    bool transportPlaying = false;
};

class TrackContextBuilder
{
public:
    static juce::var build (const TrackContextInput& input);
    static juce::String scopeToString (ContextScope scope);

private:
    static juce::var buildProjectSection (const TrackContextInput& input);
    static juce::var buildPatternSection (const Pattern& pattern, int patternIndex);
    static juce::var buildChannelSection (const PatternRow& row,
                                          int channelIndex,
                                          const LevelMeterBank* meters);
    static juce::var buildSliceSection (const SampleAsset& asset, int sliceIndex);
    static juce::var buildDerivedStats (const PatternRow& row, float peakLevel);
    static juce::var buildStepStats (const std::vector<Step>& steps);
};

} // namespace sampr