#include "ReverbProcessor.h"

namespace sampr
{

void ReverbProcessor::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (sampleRate, maxBlockSize);
    reverb.setSampleRate (sampleRate);
    prepared = true;
}

void ReverbProcessor::reset() noexcept
{
    reverb.reset();
}

void ReverbProcessor::setParameters (const ChannelReverbState& state) noexcept
{
    enabled = state.enabled;
    parameters.roomSize = juce::jlimit (0.0f, 1.0f, state.roomSize);
    parameters.damping = juce::jlimit (0.0f, 1.0f, state.damping);
    parameters.wetLevel = juce::jlimit (0.0f, 1.0f, state.wetLevel);
    parameters.dryLevel = juce::jlimit (0.0f, 1.0f, state.dryLevel);
    parameters.width = juce::jlimit (0.0f, 1.0f, state.width);
    parameters.freezeMode = 0.0f;

    if (prepared)
        reverb.setParameters (parameters);
}

void ReverbProcessor::process (juce::AudioBuffer<float>& buffer,
                               int startSample,
                               int numSamples) noexcept
{
    if (! enabled || ! prepared || numSamples <= 0)
        return;

    if (parameters.wetLevel <= 0.0001f && parameters.dryLevel <= 0.0001f)
        return;

    juce::AudioBuffer<float> wet (buffer.getNumChannels(), numSamples);
    wet.copyFrom (0, 0, buffer, 0, startSample, numSamples);

    if (buffer.getNumChannels() > 1)
        wet.copyFrom (1, 0, buffer, 1, startSample, numSamples);

    float* channels[2] = {
        wet.getWritePointer (0),
        wet.getNumChannels() > 1 ? wet.getWritePointer (1) : wet.getWritePointer (0)
    };

    reverb.processStereo (channels[0], channels[1], numSamples);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, startSample, wet, ch, 0, numSamples);
}

} // namespace sampr