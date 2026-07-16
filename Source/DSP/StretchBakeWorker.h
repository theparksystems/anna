#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <queue>

#include <juce_core/juce_core.h>

#include "../Model/SampleAsset.h"
#include "RubberBandOfflineProcessor.h"

namespace sampr
{

class AudioEngine;

struct StretchBakeJob
{
    AssetId assetId = kInvalidAssetId;
    SliceId sliceId = kInvalidSliceId;
    SharedSampleData sourceSlice;
    StretchParameters params;
};

struct StretchBakeResult
{
    AssetId assetId = kInvalidAssetId;
    SliceId sliceId = kInvalidSliceId;
    std::shared_ptr<juce::AudioBuffer<float>> bakedBuffer;
    bool success = false;
};

class StretchBakeWorker final : private juce::Thread
{
public:
    using ResultCallback = std::function<void (const StretchBakeResult&)>;

    explicit StretchBakeWorker (ResultCallback callback);
    ~StretchBakeWorker() override;

    void enqueue (StretchBakeJob job);
    void clearPending();

private:
    void run() override;

    ResultCallback onResult;
    std::mutex queueMutex;
    std::queue<StretchBakeJob> pendingJobs;
    juce::WaitableEvent jobSignal;
};

} // namespace sampr