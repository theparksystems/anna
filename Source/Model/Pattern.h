#pragma once

#include <vector>

#include <juce_graphics/juce_graphics.h>

#include "../Plugins/FxTypes.h"
#include "NoteEvent.h"
#include "SampleAsset.h"

namespace sampr
{

using PatternId = uint32_t;
inline constexpr PatternId kInvalidPatternId = 0;

struct Step
{
    bool active = false;
    float velocity = 1.0f;
    float probability = 1.0f;
};

struct PatternRow
{
    juce::String sampleRefId;
    AssetId assetId = kInvalidAssetId;
    int sliceIndex = 0;
    juce::String label;
    juce::Colour colour { 0xff4cc2ff };
    float channelGain = 1.0f;
    float channelPan = 0.0f;
    bool channelMute = false;
    bool channelSolo = false;
    ChannelEqState channelEq;
    ChannelColorState channelColor;
    ChannelCompressorState channelCompressor;
    ChannelDelayState channelDelay;
    ChannelReverbState channelReverb;
    std::vector<Step> steps;
};

struct Pattern
{
    PatternId id = kInvalidPatternId;
    juce::String name { "Pattern 1" };
    int numSteps = 16;
    int lengthBars = 1;
    std::vector<PatternRow> rows;
    std::vector<NoteEvent> notes;
};

} // namespace sampr
