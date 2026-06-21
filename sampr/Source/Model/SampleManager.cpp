#include "SampleManager.h"

#include <algorithm>

#include "../DSP/OnsetDetector.h"
#include "../DSP/PeakGenerator.h"
#include "../DSP/RubberBandOfflineProcessor.h"
#include "../DSP/RubberBandStreamProcessor.h"

namespace sampr
{

namespace
{
    SliceId nextSliceId (const SampleAsset& asset)
    {
        SliceId maxId = 0;

        for (const auto& slice : asset.slices)
            maxId = juce::jmax (maxId, slice.id);

        return maxId + 1;
    }
}

SampleManager::SampleManager (AudioEngine& engine)
    : audioEngine (engine)
{
    bakeWorker = std::make_unique<StretchBakeWorker> ([this] (const StretchBakeResult& result)
    {
        handleBakeResult (result);
    });
}

SampleManager::~SampleManager()
{
    clear();
}

void SampleManager::setChangeCallback (ChangeCallback callback)
{
    changeCallback = std::move (callback);
}

void SampleManager::notifyChanged()
{
    if (changeCallback != nullptr)
        changeCallback();
}

std::optional<AssetId> SampleManager::loadFromFile (const juce::File& file,
                                                    juce::AudioFormatManager& formatManager)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        DBG ("SAMPR: failed to create reader for " + file.getFullPathName());
        return std::nullopt;
    }

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (
        static_cast<int> (reader->numChannels),
        static_cast<int> (reader->lengthInSamples));

    if (! reader->read (buffer.get(), 0, static_cast<int> (reader->lengthInSamples), 0, true, true))
    {
        DBG ("SAMPR: failed to decode " + file.getFullPathName());
        return std::nullopt;
    }

    auto sampleData = std::make_shared<SampleData>();
    sampleData->buffer = std::move (buffer);
    sampleData->sourceSampleRate = reader->sampleRate;

    const auto assetId = nextAssetId++;

    SampleAsset asset;
    asset.id = assetId;
    asset.sourceData = sampleData;
    asset.sourceRegistryId = audioEngine.registerSample (sampleData);
    asset.filePath = file;
    asset.displayName = file.getFileNameWithoutExtension();
    asset.numChannels = sampleData->buffer->getNumChannels();
    asset.numSamples = sampleData->buffer->getNumSamples();
    asset.sampleRate = sampleData->sourceSampleRate;
    asset.peaks = PeakGenerator::generate (*sampleData->buffer);

    SliceRegion initialSlice;
    initialSlice.id = 1;
    initialSlice.startSample = 0;
    initialSlice.endSample = asset.numSamples;
    asset.slices.push_back (initialSlice);
    refreshSliceRawBuffer (asset, asset.slices.front());

    assets.emplace (assetId, std::move (asset));
    assetOrder.push_back (assetId);
    selectedAssetId = assetId;

    DBG ("SAMPR: loaded asset '" + file.getFileName() + "' id=" + juce::String (assetId));
    notifyChanged();
    return assetId;
}

void SampleManager::removeAsset (AssetId assetId)
{
    auto it = assets.find (assetId);
    if (it == assets.end())
        return;

    for (auto& slice : it->second.slices)
        invalidateSliceBake (slice, audioEngine);

    audioEngine.unregisterSample (it->second.sourceRegistryId);
    assets.erase (it);
    assetOrder.erase (std::remove (assetOrder.begin(), assetOrder.end(), assetId), assetOrder.end());

    if (selectedAssetId == assetId)
        selectedAssetId = assetOrder.empty() ? kInvalidAssetId : assetOrder.back();

    notifyChanged();
}

void SampleManager::clear()
{
    bakeWorker->clearPending();

    for (auto& [id, asset] : assets)
    {
        juce::ignoreUnused (id);
        for (auto& slice : asset.slices)
            invalidateSliceBake (slice, audioEngine);

        audioEngine.unregisterSample (asset.sourceRegistryId);
    }

    assets.clear();
    assetOrder.clear();
    selectedAssetId = kInvalidAssetId;
    notifyChanged();
}

const SampleAsset* SampleManager::getAsset (AssetId assetId) const
{
    const auto it = assets.find (assetId);
    return it != assets.end() ? &it->second : nullptr;
}

SampleAsset* SampleManager::getAsset (AssetId assetId)
{
    auto it = assets.find (assetId);
    return it != assets.end() ? &it->second : nullptr;
}

void SampleManager::setSelectedAssetId (AssetId assetId)
{
    if (assets.find (assetId) == assets.end())
        return;

    selectedAssetId = assetId;
    notifyChanged();
}

SliceRegion* SampleManager::getSelectedSlice()
{
    auto* asset = getAsset (selectedAssetId);
    if (asset == nullptr || asset->slices.empty())
        return nullptr;

    asset->selectedSliceIndex = juce::jlimit (0, static_cast<int> (asset->slices.size()) - 1,
                                              asset->selectedSliceIndex);
    return &asset->slices[static_cast<size_t> (asset->selectedSliceIndex)];
}

const SliceRegion* SampleManager::getSelectedSlice() const
{
    const auto* asset = getAsset (selectedAssetId);
    if (asset == nullptr || asset->slices.empty())
        return nullptr;

    const auto index = juce::jlimit (0, static_cast<int> (asset->slices.size()) - 1,
                                     asset->selectedSliceIndex);
    return &asset->slices[static_cast<size_t> (index)];
}

int SampleManager::addSliceAtSample (AssetId assetId, int samplePosition)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr)
        return -1;

    samplePosition = juce::jlimit (1, asset->numSamples - 1, samplePosition);

    for (const auto& slice : asset->slices)
        if (std::abs (slice.startSample - samplePosition) < 200)
            return asset->selectedSliceIndex;

    SliceRegion slice;
    slice.id = nextSliceId (*asset);
    slice.startSample = samplePosition;
    slice.endSample = asset->numSamples;
    asset->slices.push_back (slice);
    rebuildSlices (*asset);

    for (auto& s : asset->slices)
        refreshSliceRawBuffer (*asset, s);

    asset->selectedSliceIndex = static_cast<int> (asset->slices.size()) - 1;
    notifyChanged();
    return asset->selectedSliceIndex;
}

int SampleManager::autoSliceAsset (AssetId assetId, const OnsetDetectorOptions& options)
{
    auto* asset = getAsset (assetId);

    if (asset == nullptr || asset->sourceData == nullptr || asset->sourceData->buffer == nullptr)
        return 0;

    const auto& buffer = *asset->sourceData->buffer;
    const auto onsets = OnsetDetector::detect (buffer, asset->sampleRate, options);

    if (onsets.empty())
        return 0;

    for (auto& slice : asset->slices)
        invalidateSliceBake (slice, audioEngine);

    asset->slices.clear();

    std::vector<int> markers;
    markers.push_back (0);

    for (const auto onset : onsets)
    {
        if (onset <= 0 || onset >= asset->numSamples)
            continue;

        if (markers.empty() || onset - markers.back() >= 200)
            markers.push_back (onset);
    }

    SliceId nextId = 1;

    for (const auto start : markers)
    {
        SliceRegion slice;
        slice.id = nextId++;
        slice.startSample = start;
        slice.endSample = asset->numSamples;
        asset->slices.push_back (slice);
    }

    rebuildSlices (*asset);

    for (auto& slice : asset->slices)
        refreshSliceRawBuffer (*asset, slice);

    asset->selectedSliceIndex = 0;
    notifyChanged();
    return static_cast<int> (asset->slices.size());
}

void SampleManager::removeSlice (AssetId assetId, int sliceIndex)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || asset->slices.size() <= 1)
        return;

    if (! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    invalidateSliceBake (asset->slices[static_cast<size_t> (sliceIndex)], audioEngine);
    asset->slices.erase (asset->slices.begin() + sliceIndex);
    rebuildSlices (*asset);

    for (auto& slice : asset->slices)
        refreshSliceRawBuffer (*asset, slice);

    asset->selectedSliceIndex = juce::jlimit (0, static_cast<int> (asset->slices.size()) - 1,
                                              asset->selectedSliceIndex);
    notifyChanged();
}

void SampleManager::selectSlice (AssetId assetId, int sliceIndex)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr)
        return;

    if (! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    asset->selectedSliceIndex = sliceIndex;
    notifyChanged();
}

void SampleManager::updateSliceRange (AssetId assetId, int sliceIndex, int startSample, int endSample)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];
    slice.startSample = juce::jlimit (0, asset->numSamples - 1, startSample);
    slice.endSample = juce::jlimit (slice.startSample + 1, asset->numSamples, endSample);
    rebuildSlices (*asset);
    refreshSliceRawBuffer (*asset, slice);
    invalidateSliceBake (slice, audioEngine);
    notifyChanged();
}

void SampleManager::setSlicePitch (AssetId assetId, int sliceIndex, float semitones)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];
    slice.pitchSemitones = juce::jlimit (-24.0f, 24.0f, semitones);
    invalidateSliceBake (slice, audioEngine);
    notifyChanged();
}

void SampleManager::setSliceTimeRatio (AssetId assetId, int sliceIndex, float ratio)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];
    slice.timeRatio = juce::jlimit (0.25f, 4.0f, ratio);
    invalidateSliceBake (slice, audioEngine);
    notifyChanged();
}

void SampleManager::setSliceLoop (AssetId assetId, int sliceIndex, bool shouldLoop)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    asset->slices[static_cast<size_t> (sliceIndex)].loop = shouldLoop;
    notifyChanged();
}

void SampleManager::setSliceProcessingMode (AssetId assetId, int sliceIndex, StretchProcessingMode mode)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    asset->slices[static_cast<size_t> (sliceIndex)].processingMode = mode;
    notifyChanged();
}

void SampleManager::requestSliceBake (AssetId assetId, int sliceIndex)
{
    auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return;

    auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];

    if (! sliceNeedsRubberBandBake (slice))
    {
        slice.bakeReady = true;
        slice.bakeDirty = false;
        notifyChanged();
        return;
    }

    if (! RubberBandOfflineProcessor::isAvailable())
    {
        DBG ("SAMPR: Rubber Band not available; using raw playback-rate fallback");
        notifyChanged();
        return;
    }

    if (slice.bakeInProgress)
        return;

    refreshSliceRawBuffer (*asset, slice);

    StretchBakeJob job;
    job.assetId = assetId;
    job.sliceId = slice.id;
    job.sourceSlice = extractSliceData (*asset, slice);
    job.params.pitchSemitones = slice.pitchSemitones;
    job.params.timeRatio = slice.timeRatio;

    slice.bakeInProgress = true;
    slice.bakeDirty = false;
    bakeWorker->enqueue (std::move (job));
    notifyChanged();
}

void SampleManager::requestSelectedSliceBake()
{
    auto* asset = getAsset (selectedAssetId);
    if (asset == nullptr)
        return;

    requestSliceBake (selectedAssetId, asset->selectedSliceIndex);
}

std::optional<SampleId> SampleManager::resolvePlaybackSampleId (AssetId assetId, int sliceIndex) const
{
    const auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return std::nullopt;

    const auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];

    if (slice.processingMode == StretchProcessingMode::preRenderOffline
        && slice.bakeReady
        && slice.bakedRegistryId != kInvalidSampleId)
        return slice.bakedRegistryId;

    if (slice.rawRegistryId != kInvalidSampleId)
        return slice.rawRegistryId;

    return std::nullopt;
}

bool SampleManager::slicePlaybackUsesBakedBuffer (AssetId assetId, int sliceIndex) const
{
    return resolveVoicePlaybackMode (assetId, sliceIndex) == VoicePlaybackMode::timeStretch;
}

VoicePlaybackMode SampleManager::resolveVoicePlaybackMode (AssetId assetId, int sliceIndex) const
{
    const auto* asset = getAsset (assetId);

    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return VoicePlaybackMode::raw;

    const auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];

    if (slice.processingMode == StretchProcessingMode::preRenderOffline
        && slice.bakeReady
        && slice.bakedRegistryId != kInvalidSampleId)
        return VoicePlaybackMode::timeStretch;

    if (slice.processingMode == StretchProcessingMode::realTimeStream
        && sliceNeedsRubberBandBake (slice)
        && RubberBandStreamProcessor::isAvailable())
        return VoicePlaybackMode::realTimeStretch;

    return VoicePlaybackMode::raw;
}

float SampleManager::resolvePlaybackPitchSemitones (AssetId assetId, int sliceIndex) const
{
    if (slicePlaybackUsesBakedBuffer (assetId, sliceIndex))
        return 0.0f;

    const auto* asset = getAsset (assetId);
    if (asset == nullptr)
        return 0.0f;

    return asset->slices[static_cast<size_t> (sliceIndex)].pitchSemitones;
}

float SampleManager::resolvePlaybackTimeRatio (AssetId assetId, int sliceIndex) const
{
    const auto* asset = getAsset (assetId);

    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return 1.0f;

    const auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];

    if (resolveVoicePlaybackMode (assetId, sliceIndex) == VoicePlaybackMode::realTimeStretch)
        return slice.timeRatio;

    return 1.0f;
}

bool SampleManager::resolvePlaybackLoop (AssetId assetId, int sliceIndex) const
{
    const auto* asset = getAsset (assetId);
    if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
        return false;

    return asset->slices[static_cast<size_t> (sliceIndex)].loop;
}

void SampleManager::rebuildSlices (SampleAsset& asset)
{
    std::sort (asset.slices.begin(), asset.slices.end(),
               [] (const SliceRegion& a, const SliceRegion& b)
               {
                   return a.startSample < b.startSample;
               });

    for (size_t i = 0; i < asset.slices.size(); ++i)
    {
        auto& slice = asset.slices[i];
        slice.startSample = juce::jlimit (0, asset.numSamples - 1, slice.startSample);

        if (i + 1 < asset.slices.size())
            slice.endSample = asset.slices[i + 1].startSample;
        else
            slice.endSample = juce::jlimit (slice.startSample + 1, asset.numSamples, slice.endSample);

        slice.endSample = juce::jmax (slice.startSample + 1, slice.endSample);
    }
}

SharedSampleData SampleManager::extractSliceData (const SampleAsset& asset, const SliceRegion& slice) const
{
    if (asset.sourceData == nullptr || asset.sourceData->buffer == nullptr)
        return nullptr;

    const auto& source = *asset.sourceData->buffer;
    const auto length = slice.endSample - slice.startSample;

    if (length <= 0)
        return nullptr;

    auto sliceBuffer = std::make_shared<juce::AudioBuffer<float>> (source.getNumChannels(), length);

    for (int ch = 0; ch < source.getNumChannels(); ++ch)
        sliceBuffer->copyFrom (ch, 0, source, ch, slice.startSample, length);

    auto data = std::make_shared<SampleData>();
    data->buffer = std::move (sliceBuffer);
    data->sourceSampleRate = asset.sampleRate;
    return data;
}

void SampleManager::refreshSliceRawBuffer (SampleAsset& asset, SliceRegion& slice)
{
    if (slice.rawRegistryId != kInvalidSampleId)
        audioEngine.unregisterSample (slice.rawRegistryId);

    const auto data = extractSliceData (asset, slice);

    if (data == nullptr)
    {
        slice.rawRegistryId = kInvalidSampleId;
        return;
    }

    slice.rawRegistryId = audioEngine.registerSample (data);
    invalidateSliceBake (slice, audioEngine);
}

void SampleManager::invalidateSliceBake (SliceRegion& slice, AudioEngine& engine)
{
    if (slice.bakedRegistryId != kInvalidSampleId)
        engine.unregisterSample (slice.bakedRegistryId);

    slice.bakedRegistryId = kInvalidSampleId;
    slice.bakeReady = false;
    slice.bakeInProgress = false;
    slice.bakeDirty = true;
}

void SampleManager::handleBakeResult (const StretchBakeResult& result)
{
    auto* asset = getAsset (result.assetId);
    if (asset == nullptr)
        return;

    auto* slice = static_cast<SliceRegion*> (nullptr);

    for (auto& candidate : asset->slices)
    {
        if (candidate.id == result.sliceId)
        {
            slice = &candidate;
            break;
        }
    }

    if (slice == nullptr)
        return;

    slice->bakeInProgress = false;

    if (! result.success || result.bakedBuffer == nullptr)
    {
        DBG ("SAMPR: stretch bake failed for slice " + juce::String (result.sliceId));
        notifyChanged();
        return;
    }

    auto bakedData = std::make_shared<SampleData>();
    bakedData->buffer = result.bakedBuffer;
    bakedData->sourceSampleRate = asset->sampleRate;

    if (slice->bakedRegistryId != kInvalidSampleId)
        audioEngine.unregisterSample (slice->bakedRegistryId);

    slice->bakedRegistryId = audioEngine.registerSample (bakedData);
    slice->bakeReady = slice->bakedRegistryId != kInvalidSampleId;
    slice->bakeDirty = ! slice->bakeReady;

    DBG ("SAMPR: baked slice " + juce::String (slice->id)
         + " -> registry id " + juce::String (slice->bakedRegistryId));

    notifyChanged();
}

void SampleManager::exportSampleRefs (std::vector<SampleRefData>& refsOut,
                                      const juce::File& projectRoot) const
{
    refsOut.clear();

    for (const auto assetId : assetOrder)
    {
        const auto* asset = getAsset (assetId);
        if (asset == nullptr)
            continue;

        SampleRefData ref;
        ref.refId = asset->refId.isNotEmpty() ? asset->refId : ("asset-" + juce::String (assetId));
        ref.displayName = asset->displayName;

        if (projectRoot.exists() && asset->filePath.exists())
            ref.relativePath = asset->filePath.getRelativePathFrom (projectRoot.getParentDirectory());
        else
            ref.relativePath = asset->filePath.getFullPathName();

        ref.selectedSliceIndex = asset->selectedSliceIndex;

        for (const auto& slice : asset->slices)
        {
            SliceRefData sliceRef;
            sliceRef.id = slice.id;
            sliceRef.startSample = slice.startSample;
            sliceRef.endSample = slice.endSample;
            sliceRef.pitchSemitones = slice.pitchSemitones;
            sliceRef.timeRatio = slice.timeRatio;
            sliceRef.loop = slice.loop;
            sliceRef.processingMode = slice.processingMode;
            ref.slices.push_back (sliceRef);
        }

        refsOut.push_back (std::move (ref));
    }
}

std::unordered_map<juce::String, AssetId> SampleManager::loadSamplesFromRefs (
    const std::vector<SampleRefData>& refs,
    const juce::File& projectFile,
    juce::AudioFormatManager& formatManager)
{
    std::unordered_map<juce::String, AssetId> refMap;
    const auto projectRoot = projectFile.getParentDirectory();

    for (const auto& ref : refs)
    {
        juce::File sampleFile;

        if (ref.relativePath.isNotEmpty())
            sampleFile = projectRoot.getChildFile (ref.relativePath);

        if (! sampleFile.existsAsFile())
            sampleFile = juce::File (ref.relativePath);

        if (! sampleFile.existsAsFile())
        {
            DBG ("SAMPR: missing sample for ref " + ref.refId + " -> " + ref.relativePath);
            continue;
        }

        const auto assetIdOpt = loadFromFile (sampleFile, formatManager);

        if (! assetIdOpt.has_value())
            continue;

        auto* asset = getAsset (*assetIdOpt);

        if (asset == nullptr)
            continue;

        asset->refId = ref.refId;
        asset->displayName = ref.displayName.isNotEmpty() ? ref.displayName : asset->displayName;
        applySliceRefData (*asset, ref.slices);
        asset->selectedSliceIndex = juce::jlimit (0, juce::jmax (0, static_cast<int> (asset->slices.size()) - 1),
                                                ref.selectedSliceIndex);
        refMap[ref.refId] = *assetIdOpt;
    }

    return refMap;
}

std::unordered_map<juce::String, AssetId> SampleManager::buildRefIdToAssetMap() const
{
    std::unordered_map<juce::String, AssetId> refMap;

    for (const auto assetId : assetOrder)
    {
        if (const auto* asset = getAsset (assetId); asset != nullptr && asset->refId.isNotEmpty())
            refMap[asset->refId] = assetId;
    }

    return refMap;
}

void SampleManager::remapPatternAssetIds (ProjectModel& project,
                                          const std::unordered_map<juce::String, AssetId>& refMap) const
{
    for (auto& pattern : project.getPatterns())
    {
        for (auto& row : pattern.rows)
        {
            if (row.sampleRefId.isNotEmpty())
            {
                if (const auto it = refMap.find (row.sampleRefId); it != refMap.end())
                    row.assetId = it->second;
                else
                    row.assetId = kInvalidAssetId;
            }
        }

        for (auto& note : pattern.notes)
        {
            if (note.sampleRefId.isNotEmpty())
            {
                if (const auto it = refMap.find (note.sampleRefId); it != refMap.end())
                    note.assetId = it->second;
                else
                    note.assetId = kInvalidAssetId;
            }
        }
    }
}

void SampleManager::applySliceRefData (SampleAsset& asset, const std::vector<SliceRefData>& sliceRefs)
{
    if (sliceRefs.empty())
        return;

    for (auto& slice : asset.slices)
        invalidateSliceBake (slice, audioEngine);

    asset.slices.clear();

    for (const auto& ref : sliceRefs)
    {
        SliceRegion slice;
        slice.id = ref.id != kInvalidSliceId ? ref.id : nextSliceId (asset);
        slice.startSample = ref.startSample;
        slice.endSample = ref.endSample;
        slice.pitchSemitones = ref.pitchSemitones;
        slice.timeRatio = ref.timeRatio;
        slice.loop = ref.loop;
        slice.processingMode = ref.processingMode;
        asset.slices.push_back (slice);
    }

    rebuildSlices (asset);

    for (auto& slice : asset.slices)
        refreshSliceRawBuffer (asset, slice);
}

} // namespace sampr