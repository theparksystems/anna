#include "ReverbProcessor.h"

namespace sampr
{

void ReverbProcessor::prepare (double sampleRate, int maxBlockSize)
{
    reverb.setSampleRate (sampleRate);
    preparedBlockSize = juce::jmax (1, maxBlockSize);
    wetBuffer.setSize (2, preparedBlockSize, false, true, true);
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

    const auto numChannels = juce::jmin (2, buffer.getNumChannels());

    if (numChannels <= 0)
        return;

    if (numSamples > wetBuffer.getNumSamples() || numChannels > wetBuffer.getNumChannels())
        return;

    wetBuffer.clear (0, numSamples);
    wetBuffer.copyFrom (0, 0, buffer, 0, startSample, numSamples);

    if (numChannels > 1)
        wetBuffer.copyFrom (1, 0, buffer, 1, startSample, numSamples);

    float* channelPtrs[2] = {
        wetBuffer.getWritePointer (0),
        numChannels > 1 ? wetBuffer.getWritePointer (1) : wetBuffer.getWritePointer (0)
    };

    reverb.processStereo (channelPtrs[0], channelPtrs[1], numSamples);

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.copyFrom (ch, startSample, wetBuffer, ch, 0, numSamples);
}

} // namespace sampr
