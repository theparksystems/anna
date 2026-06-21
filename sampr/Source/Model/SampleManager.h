#pragma once

#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Audio/AudioEngine.h"
#include "../Audio/SampleVoice.h"
#include "../DSP/OnsetDetector.h"
#include "../DSP/StretchBakeWorker.h"
#include "ProjectModel.h"
#include "SampleAsset.h"
#include "SampleRefData.h"

namespace sampr
{

class SampleManager
{
public:
    using ChangeCallback = std::function<void()>;

    explicit SampleManager (AudioEngine& engine);
    ~SampleManager();

    void setChangeCallback (ChangeCallback callback);

    std::optional<AssetId> loadFromFile (const juce::File& file,
                                         juce::AudioFormatManager& formatManager);

    void removeAsset (AssetId assetId);
    void clear();

    const std::vector<AssetId>& getAssetIds() const noexcept { return assetOrder; }
    const SampleAsset* getAsset (AssetId assetId) const;
    SampleAsset* getAsset (AssetId assetId);

    AssetId getSelectedAssetId() const noexcept { return selectedAssetId; }
    void setSelectedAssetId (AssetId assetId);

    SliceRegion* getSelectedSlice();
    const SliceRegion* getSelectedSlice() const;

    int addSliceAtSample (AssetId assetId, int samplePosition);
    int autoSliceAsset (AssetId assetId, const OnsetDetectorOptions& options = {});
    void removeSlice (AssetId assetId, int sliceIndex);
    void selectSlice (AssetId assetId, int sliceIndex);
    void updateSliceRange (AssetId assetId, int sliceIndex, int startSample, int endSample);

    void setSlicePitch (AssetId assetId, int sliceIndex, float semitones);
    void setSliceTimeRatio (AssetId assetId, int sliceIndex, float ratio);
    void setSliceLoop (AssetId assetId, int sliceIndex, bool shouldLoop);
    void setSliceProcessingMode (AssetId assetId, int sliceIndex, StretchProcessingMode mode);

    void requestSliceBake (AssetId assetId, int sliceIndex);
    void requestSelectedSliceBake();

    std::optional<SampleId> resolvePlaybackSampleId (AssetId assetId, int sliceIndex) const;
    bool slicePlaybackUsesBakedBuffer (AssetId assetId, int sliceIndex) const;
    VoicePlaybackMode resolveVoicePlaybackMode (AssetId assetId, int sliceIndex) const;
    float resolvePlaybackPitchSemitones (AssetId assetId, int sliceIndex) const;
    float resolvePlaybackTimeRatio (AssetId assetId, int sliceIndex) const;
    bool resolvePlaybackLoop (AssetId assetId, int sliceIndex) const;

    void exportSampleRefs (std::vector<SampleRefData>& refsOut, const juce::File& projectRoot) const;
    std::unordered_map<juce::String, AssetId> loadSamplesFromRefs (const std::vector<SampleRefData>& refs,
                                                                   const juce::File& projectFile,
                                                                   juce::AudioFormatManager& formatManager);
    std::unordered_map<juce::String, AssetId> buildRefIdToAssetMap() const;
    void remapPatternAssetIds (ProjectModel& project,
                               const std::unordered_map<juce::String, AssetId>& refMap) const;
    void applySliceRefData (SampleAsset& asset, const std::vector<SliceRefData>& sliceRefs);

private:
    void rebuildSlices (SampleAsset& asset);
    void refreshSliceRawBuffer (SampleAsset& asset, SliceRegion& slice);
    void invalidateSliceBake (SliceRegion& slice, AudioEngine& engine);
    void handleBakeResult (const StretchBakeResult& result);
    SharedSampleData extractSliceData (const SampleAsset& asset, const SliceRegion& slice) const;
    void notifyChanged();

    AudioEngine& audioEngine;
    ChangeCallback changeCallback;
    std::unique_ptr<StretchBakeWorker> bakeWorker;

    std::unordered_map<AssetId, SampleAsset> assets;
    std::vector<AssetId> assetOrder;
    AssetId nextAssetId { 1 };
    AssetId selectedAssetId = kInvalidAssetId;
};

} // namespace sampr