#include "SampleVoice.h"

#include "../DSP/RubberBandStreamProcessor.h"
#include "LevelMeterBank.h"

#include <cmath>

namespace sampr
{

namespace
{
    void applyPan (float pan, float gain, float& left, float& right) noexcept
    {
        const auto panClamped = juce::jlimit (-1.0f, 1.0f, pan);
        const auto angle = (panClamped + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        left = gain * std::cos (angle);
        right = gain * std::sin (angle);
    }
}

void SampleVoice::prepare (double sampleRate, int maxBlockSize) noexcept
{
    juce::ignoreUnused (maxBlockSize);
    deviceSampleRate = sampleRate;
}

void SampleVoice::trigger (SharedSampleData sampleData, const VoiceRenderParams& renderParams) noexcept
{
    if (sampleData == nullptr || sampleData->buffer == nullptr || sampleData->buffer->getNumSamples() == 0)
        return;

    stop();

    sample = std::move (sampleData);
    params = renderParams;
    channelIndex = renderParams.channelIndex;
    playhead = 0.0;

    if (sample->sourceSampleRate > 0.0)
        sourceToDeviceRatio = sample->sourceSampleRate / deviceSampleRate;
    else
        sourceToDeviceRatio = 1.0;

    if (params.mode == VoicePlaybackMode::realTimeStretch
        && RubberBandStreamProcessor::isAvailable()
        && sample->buffer != nullptr)
    {
        streamProcessor = std::make_unique<RubberBandStreamProcessor>();
        streamProcessor->begin (*sample->buffer,
                                sample->sourceSampleRate > 0.0 ? sample->sourceSampleRate : deviceSampleRate,
                                params.timeRatio,
                                params.pitchSemitones);
    }

    active.store (true, std::memory_order_release);
}

void SampleVoice::stop() noexcept
{
    active.store (false, std::memory_order_release);
    sample.reset();
    streamProcessor.reset();
    playhead = 0.0;
    channelIndex = -1;
}

float SampleVoice::readInterpolated (const juce::AudioBuffer<float>& buffer,
                                     int channel,
                                     double position) const noexcept
{
    const auto maxIndex = buffer.getNumSamples() - 1;

    if (maxIndex <= 0)
        return buffer.getSample (channel, 0);

    const auto index = static_cast<int> (position);
    const auto frac = static_cast<float> (position - static_cast<double> (index));
    const auto i0 = juce::jlimit (0, maxIndex, index);
    const auto i1 = juce::jmin (i0 + 1, maxIndex);

    const auto s0 = buffer.getSample (channel, i0);
    const auto s1 = buffer.getSample (channel, i1);
    return s0 + (s1 - s0) * frac;
}

void SampleVoice::renderRawAdding (juce::AudioBuffer<float>& output,
                                   int startSample,
                                   int numSamples,
                                   LevelMeterBank* meters) noexcept
{
    if (sample == nullptr || sample->buffer == nullptr)
    {
        stop();
        return;
    }

    const auto& buffer = *sample->buffer;
    const auto numSourceSamples = buffer.getNumSamples();

    if (numSourceSamples <= 0)
    {
        stop();
        return;
    }

    float leftGain = 1.0f;
    float rightGain = 1.0f;
    applyPan (params.pan, params.gain, leftGain, rightGain);

    const auto pitchRatio = params.mode == VoicePlaybackMode::timeStretch ? 1.0f : params.pitchRatio;
    const auto increment = static_cast<double> (pitchRatio) * sourceToDeviceRatio;
    const auto numOutputChannels = output.getNumChannels();
    const auto numSourceChannels = buffer.getNumChannels();
    float blockPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (playhead >= static_cast<double> (numSourceSamples - 1))
        {
            if (params.loop)
            {
                playhead = 0.0;
                if (numSourceSamples <= 1)
                    break;
            }
            else
            {
                stop();
                break;
            }
        }

        const auto sourceLeft = readInterpolated (buffer, 0, playhead);
        const auto sourceRight = numSourceChannels > 1
                                   ? readInterpolated (buffer, 1, playhead)
                                   : sourceLeft;

        const auto outLeft = sourceLeft * leftGain;
        const auto outRight = sourceRight * rightGain;

        if (numOutputChannels > 0)
            output.addSample (0, startSample + i, outLeft);

        if (numOutputChannels > 1)
            output.addSample (1, startSample + i, outRight);

        blockPeak = std::max (blockPeak, std::abs (outLeft));
        blockPeak = std::max (blockPeak, std::abs (outRight));
        playhead += increment;
    }

    if (meters != nullptr)
    {
        if (channelIndex >= 0)
            meters->submitPeak (channelIndex, blockPeak);

        meters->submitMasterPeak (blockPeak);
    }
}

void SampleVoice::renderRealTimeStretchAdding (juce::AudioBuffer<float>& output,
                                               int startSample,
                                               int numSamples,
                                               LevelMeterBank* meters) noexcept
{
    if (streamProcessor == nullptr)
    {
        renderRawAdding (output, startSample, numSamples, meters);
        return;
    }

    float leftGain = 1.0f;
    float rightGain = 1.0f;
    applyPan (params.pan, params.gain, leftGain, rightGain);

    const auto numOutputChannels = output.getNumChannels();
    float blockPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float sourceLeft = 0.0f;
        float sourceRight = 0.0f;

        if (! streamProcessor->renderSample (sourceLeft, sourceRight))
        {
            stop();
            break;
        }

        const auto outLeft = sourceLeft * leftGain;
        const auto outRight = sourceRight * rightGain;

        if (numOutputChannels > 0)
            output.addSample (0, startSample + i, outLeft);

        if (numOutputChannels > 1)
            output.addSample (1, startSample + i, outRight);

        blockPeak = std::max (blockPeak, std::abs (outLeft));
        blockPeak = std::max (blockPeak, std::abs (outRight));
    }

    if (meters != nullptr)
    {
        if (channelIndex >= 0)
            meters->submitPeak (channelIndex, blockPeak);

        meters->submitMasterPeak (blockPeak);
    }
}

void SampleVoice::renderAdding (juce::AudioBuffer<float>& output,
                                int startSample,
                                int numSamples,
                                LevelMeterBank* meters) noexcept
{
    if (! active.load (std::memory_order_acquire))
        return;

    if (params.mode == VoicePlaybackMode::realTimeStretch)
        renderRealTimeStretchAdding (output, startSample, numSamples, meters);
    else
        renderRawAdding (output, startSample, numSamples, meters);
}

} // namespace sampr