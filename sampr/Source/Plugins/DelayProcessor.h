#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "FxTypes.h"

namespace sampr
{

class DelayProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const ChannelDelayState& state) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    void ensureDelayCapacity (int delaySamples);

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writePosition = 0;
    int delaySamples = 0;
    double sampleRate = 44100.0;
    bool enabled = false;
    float feedback = 0.0f;
    float mix = 0.0f;
};

} // namespace sampr