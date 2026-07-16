#include "Mixer.h"

namespace sampr
{

void Mixer::setMasterGain (float gain) noexcept
{
    masterGain.store (juce::jlimit (0.0f, 2.0f, gain), std::memory_order_relaxed);
}

float Mixer::getMasterGain() const noexcept
{
    return masterGain.load (std::memory_order_relaxed);
}

void Mixer::processMaster (juce::AudioBuffer<float>& buffer,
                           int startSample,
                           int numSamples) const noexcept
{
    const auto gain = masterGain.load (std::memory_order_relaxed);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.applyGain (ch, startSample, numSamples, gain);
}

} // namespace sampr