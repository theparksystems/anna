#pragma once

#include <array>
#include <atomic>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Plugins/CompressorProcessor.h"
#include "../Plugins/DelayProcessor.h"
#include "../Plugins/ParametricEqProcessor.h"
#include "../Plugins/ReverbProcessor.h"
#include "FxSnapshot.h"
#include "PatternSnapshot.h"

namespace sampr
{

class ChannelMixer
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void setFxSnapshot (SharedFxSnapshot snapshot) noexcept;
    void beginBlock (int startSample, int numSamples) noexcept;
    juce::AudioBuffer<float>* getWritableChannelBuffer (int channelIndex) noexcept;
    void mixToMaster (juce::AudioBuffer<float>& master, int startSample, int numSamples) noexcept;

private:
    void syncFxFromSnapshot() noexcept;

    std::array<juce::AudioBuffer<float>, kMaxPatternChannels> channelBuffers;
    std::array<ParametricEqProcessor, kMaxPatternChannels> eqProcessors;
    std::array<CompressorProcessor, kMaxPatternChannels> compressorProcessors;
    std::array<DelayProcessor, kMaxPatternChannels> delayProcessors;
    std::array<ReverbProcessor, kMaxPatternChannels> reverbProcessors;
    std::array<ChannelFxState, kMaxPatternChannels> lastFxState {};
    std::atomic<std::shared_ptr<const FxSnapshot>> fxSnapshot;
    int preparedBlockSize = 0;
    bool fxStateInitialized = false;
};

} // namespace sampr