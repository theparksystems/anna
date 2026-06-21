#include "DelayProcessor.h"

namespace sampr
{

void DelayProcessor::prepare (double sampleRateIn, int maxBlockSize)
{
    sampleRate = sampleRateIn;
    const auto maxDelaySamples = static_cast<int> (sampleRate * 2.0) + maxBlockSize + 8;
    delayBufferL.assign (static_cast<size_t> (maxDelaySamples), 0.0f);
    delayBufferR.assign (static_cast<size_t> (maxDelaySamples), 0.0f);
    writePosition = 0;
    reset();
}

void DelayProcessor::reset() noexcept
{
    std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
    std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
    writePosition = 0;
}

void DelayProcessor::setParameters (const ChannelDelayState& state) noexcept
{
    enabled = state.enabled;
    feedback = juce::jlimit (0.0f, 0.95f, state.feedback);
    mix = juce::jlimit (0.0f, 1.0f, state.mix);

    const auto timeMs = juce::jlimit (1.0f, 2000.0f, state.timeMs);
    delaySamples = juce::jmax (1, static_cast<int> (sampleRate * static_cast<double> (timeMs) * 0.001));
    ensureDelayCapacity (delaySamples + 8);
}

void DelayProcessor::ensureDelayCapacity (int requiredSamples)
{
    if (static_cast<int> (delayBufferL.size()) >= requiredSamples)
        return;

    const auto newSize = static_cast<size_t> (requiredSamples);
    delayBufferL.resize (newSize, 0.0f);
    delayBufferR.resize (newSize, 0.0f);
    writePosition = 0;
}

void DelayProcessor::process (juce::AudioBuffer<float>& buffer,
                                int startSample,
                                int numSamples) noexcept
{
    if (! enabled || numSamples <= 0 || delaySamples <= 0
        || delayBufferL.empty() || mix <= 0.0001f)
        return;

    const auto bufferSize = static_cast<int> (delayBufferL.size());
    auto* left = buffer.getWritePointer (0, startSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : left;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto readPosition = (writePosition - delaySamples + bufferSize) % bufferSize;
        const auto delayedL = delayBufferL[static_cast<size_t> (readPosition)];
        const auto delayedR = delayBufferR[static_cast<size_t> (readPosition)];

        const auto dryL = left[i];
        const auto dryR = right[i];
        const auto wetL = dryL + delayedL;
        const auto wetR = dryR + delayedR;

        left[i] = dryL * (1.0f - mix) + wetL * mix;
        right[i] = dryR * (1.0f - mix) + wetR * mix;

        delayBufferL[static_cast<size_t> (writePosition)] = wetL + delayedL * feedback;
        delayBufferR[static_cast<size_t> (writePosition)] = wetR + delayedR * feedback;
        writePosition = (writePosition + 1) % bufferSize;
    }
}

} // namespace sampr