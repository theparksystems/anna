#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "SampleOriginMetadata.h"

#include <cstdint>

namespace sampr
{

using SliceId = uint32_t;
inline constexpr SliceId kInvalidSliceId = 0;

enum class StretchProcessingMode
{
    preRenderOffline,
    rawPlaybackRate,
    realTimeStream,
};

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
    SampleOriginMetadata origin;
    std::vector<SliceRefData> slices;
    int selectedSliceIndex = 0;
};

} // namespace sampr
