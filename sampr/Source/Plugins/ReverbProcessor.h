#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "FxTypes.h"

namespace sampr
{

class ReverbProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const ChannelReverbState& state) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters parameters;
    bool enabled = false;
    bool prepared = false;
};

} // namespace sampr