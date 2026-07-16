#pragma once

#include <juce_dsp/juce_dsp.h>

#include "FxTypes.h"

namespace sampr
{

class CompressorProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const ChannelCompressorState& state) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    juce::dsp::Compressor<float> compressor;
    juce::dsp::ProcessSpec processSpec;
    bool enabled = false;
    bool prepared = false;
    float makeupGain = 1.0f;
};

} // namespace sampr