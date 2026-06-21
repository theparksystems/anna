#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "../Audio/AudioTypes.h"

namespace sampr
{

using AssetId = uint32_t;
inline constexpr AssetId kInvalidAssetId = 0;

using SliceId = uint32_t;
inline constexpr SliceId kInvalidSliceId = 0;

struct WaveformPeaks
{
    std::vector<float> minMax;
    int numColumns = 0;
};

enum class StretchProcessingMode
{
    preRenderOffline,
    rawPlaybackRate,
    realTimeStream,
};

struct SliceRegion
{
    SliceId id = kInvalidSliceId;
    int startSample = 0;
    int endSample = 0;
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;
    bool loop = false;
    StretchProcessingMode processingMode = StretchProcessingMode::preRenderOffline;

    SampleId rawRegistryId = kInvalidSampleId;
    SampleId bakedRegistryId = kInvalidSampleId;
    bool bakeInProgress = false;
    bool bakeReady = false;
    bool bakeDirty = true;
};

struct SampleAsset
{
    AssetId id = kInvalidAssetId;
    juce::String refId;
    SampleId sourceRegistryId = kInvalidSampleId;
    SharedSampleData sourceData;

    juce::File filePath;
    juce::String displayName;
    int numChannels = 0;
    int numSamples = 0;
    double sampleRate = 0.0;

    WaveformPeaks peaks;
    std::vector<SliceRegion> slices;
    int selectedSliceIndex = 0;
};

inline bool sliceNeedsRubberBandBake (const SliceRegion& slice) noexcept
{
    return std::abs (slice.pitchSemitones) > 0.01f
           || std::abs (slice.timeRatio - 1.0f) > 0.01f;
}

} // namespace sampr