#include "CompressorProcessor.h"

namespace sampr
{

void CompressorProcessor::prepare (double sampleRate, int maxBlockSize)
{
    processSpec.sampleRate = sampleRate;
    processSpec.maximumBlockSize = static_cast<juce::uint32> (juce::jmax (1, maxBlockSize));
    processSpec.numChannels = 2;
    compressor.prepare (processSpec);
    prepared = true;
}

void CompressorProcessor::reset() noexcept
{
    compressor.reset();
}

void CompressorProcessor::setParameters (const ChannelCompressorState& state) noexcept
{
    enabled = state.enabled;
    makeupGain = juce::Decibels::decibelsToGain (state.makeupGainDb);

    if (! prepared)
        return;

    compressor.setThreshold (state.thresholdDb);
    compressor.setRatio (juce::jmax (1.0f, state.ratio));
    compressor.setAttack (juce::jmax (0.1f, state.attackMs));
    compressor.setRelease (juce::jmax (1.0f, state.releaseMs));
}

void CompressorProcessor::process (juce::AudioBuffer<float>& buffer,
                                   int startSample,
                                   int numSamples) noexcept
{
    if (! enabled || ! prepared || numSamples <= 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    auto subBlock = block.getSubBlock (static_cast<size_t> (startSample),
                                       static_cast<size_t> (numSamples));
    juce::dsp::ProcessContextReplacing<float> context (subBlock);
    compressor.process (context);

    if (std::abs (makeupGain - 1.0f) > 0.0001f)
        subBlock.multiplyBy (makeupGain);
}

} // namespace sampr