#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "SampleAsset.h"

namespace sampr
{

struct SliceRefData
{
    SliceId id = kInvalidSliceId;
    int startSample = 0;
    int endSample = 0;
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;
    bool loop = false;
    StretchProcessingMode processingMode = StretchProcessingMode::preRenderOffline;
};

struct SampleRefData
{
    juce::String refId;
    juce::String relativePath;
    juce::String displayName;
    std::vector<SliceRefData> slices;
    int selectedSliceIndex = 0;
};

} // namespace sampr