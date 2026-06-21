#pragma once

#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

class Mixer
{
public:
    void setMasterGain (float gain) noexcept;
    float getMasterGain() const noexcept;

    void processMaster (juce::AudioBuffer<float>& buffer,
                        int startSample,
                        int numSamples) const noexcept;

private:
    std::atomic<float> masterGain { 0.9f };
};

} // namespace sampr