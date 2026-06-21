#include "ChannelMixer.h"

namespace sampr
{

void ChannelMixer::prepare (double sampleRate, int maxBlockSize)
{
    preparedBlockSize = juce::jmax (1, maxBlockSize);

    for (int i = 0; i < kMaxPatternChannels; ++i)
    {
        channelBuffers[static_cast<size_t> (i)].setSize (2, preparedBlockSize, false, true, true);
        eqProcessors[static_cast<size_t> (i)].prepare (sampleRate, preparedBlockSize);
        compressorProcessors[static_cast<size_t> (i)].prepare (sampleRate, preparedBlockSize);
        delayProcessors[static_cast<size_t> (i)].prepare (sampleRate, preparedBlockSize);
        reverbProcessors[static_cast<size_t> (i)].prepare (sampleRate, preparedBlockSize);
    }

    fxStateInitialized = false;
}

void ChannelMixer::setFxSnapshot (SharedFxSnapshot snapshot) noexcept
{
    fxSnapshot.store (std::move (snapshot), std::memory_order_release);
}

void ChannelMixer::syncFxFromSnapshot() noexcept
{
    const auto snap = fxSnapshot.load (std::memory_order_acquire);

    if (snap == nullptr)
        return;

    for (int ch = 0; ch < kMaxPatternChannels; ++ch)
    {
        const auto& nextState = snap->channels[static_cast<size_t> (ch)];

        if (! fxStateInitialized || nextState != lastFxState[static_cast<size_t> (ch)])
        {
            eqProcessors[static_cast<size_t> (ch)].setParameters (nextState.eq);
            compressorProcessors[static_cast<size_t> (ch)].setParameters (nextState.compressor);
            delayProcessors[static_cast<size_t> (ch)].setParameters (nextState.delay);
            reverbProcessors[static_cast<size_t> (ch)].setParameters (nextState.reverb);
            lastFxState[static_cast<size_t> (ch)] = nextState;
        }
    }

    fxStateInitialized = true;
}

void ChannelMixer::beginBlock (int startSample, int numSamples) noexcept
{
    juce::ignoreUnused (startSample);

    for (int i = 0; i < kMaxPatternChannels; ++i)
        channelBuffers[static_cast<size_t> (i)].clear (0, numSamples);
}

juce::AudioBuffer<float>* ChannelMixer::getWritableChannelBuffer (int channelIndex) noexcept
{
    if (! juce::isPositiveAndBelow (channelIndex, kMaxPatternChannels))
        return nullptr;

    return &channelBuffers[static_cast<size_t> (channelIndex)];
}

void ChannelMixer::mixToMaster (juce::AudioBuffer<float>& master,
                                int startSample,
                                int numSamples) noexcept
{
    syncFxFromSnapshot();

    for (int ch = 0; ch < kMaxPatternChannels; ++ch)
    {
        auto& channelBuffer = channelBuffers[static_cast<size_t> (ch)];
        eqProcessors[static_cast<size_t> (ch)].process (channelBuffer, 0, numSamples);
        compressorProcessors[static_cast<size_t> (ch)].process (channelBuffer, 0, numSamples);
        delayProcessors[static_cast<size_t> (ch)].process (channelBuffer, 0, numSamples);
        reverbProcessors[static_cast<size_t> (ch)].process (channelBuffer, 0, numSamples);

        const auto channelsToMix = juce::jmin (master.getNumChannels(), channelBuffer.getNumChannels());

        for (int c = 0; c < channelsToMix; ++c)
            master.addFrom (c, startSample, channelBuffer, c, 0, numSamples);
    }
}

} // namespace sampr