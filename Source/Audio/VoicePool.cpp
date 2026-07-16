#include "VoicePool.h"

#include "ChannelMixer.h"

namespace sampr
{

void VoicePool::prepare (double sampleRate, int maxBlockSize) noexcept
{
    for (auto& voice : voices)
        voice.prepare (sampleRate, maxBlockSize);
}

void VoicePool::reset() noexcept
{
    stopAll();
}

void VoicePool::triggerSample (const SampleRegistry& registry,
                               SampleId sampleId,
                               float velocity,
                               float gain,
                               float pan,
                               float pitchSemitones,
                               float timeRatio,
                               bool loop,
                               VoicePlaybackMode mode,
                               int channelIndex,
                               AudioDiagnostics& diagnostics) noexcept
{
    const auto sampleData = registry.getSample (sampleId);

    if (sampleData == nullptr)
    {
        diagnostics.postFromAudioThread (AudioLogCode::sampleNotFound);
        return;
    }

    auto voiceIndex = findFreeVoice();

    if (voiceIndex < 0)
    {
        voiceIndex = findVoiceToSteal();
        diagnostics.postFromAudioThread (AudioLogCode::voicePoolExhausted);
    }

    VoiceRenderParams params;
    params.gain = gain * velocityToGain (velocity);
    params.pan = pan;
    params.pitchSemitones = pitchSemitones;
    params.pitchRatio = semitonesToRatio (pitchSemitones);
    params.timeRatio = juce::jlimit (0.25f, 4.0f, timeRatio);
    params.loop = loop;
    params.mode = mode;
    params.channelIndex = channelIndex;

    voices[static_cast<size_t> (voiceIndex)].trigger (sampleData, params);
}

void VoicePool::stopAll() noexcept
{
    for (auto& voice : voices)
        voice.stop();
}

int VoicePool::getActiveVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& voice : voices)
        if (voice.isActive())
            ++count;

    return count;
}

void VoicePool::renderAdding (juce::AudioBuffer<float>& masterOutput,
                              ChannelMixer* channelMixer,
                              int startSample,
                              int numSamples,
                              LevelMeterBank* meters) noexcept
{
    for (auto& voice : voices)
    {
        juce::AudioBuffer<float>* target = &masterOutput;

        if (channelMixer != nullptr)
        {
            const auto channelIndex = voice.getChannelIndex();

            if (channelIndex >= 0)
                if (auto* channelBuffer = channelMixer->getWritableChannelBuffer (channelIndex))
                    target = channelBuffer;
        }

        voice.renderAdding (*target, startSample, numSamples, meters);
    }
}

int VoicePool::findFreeVoice() const noexcept
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (! voices[static_cast<size_t> (i)].isActive())
            return i;

    return -1;
}

int VoicePool::findVoiceToSteal() noexcept
{
    const auto index = static_cast<int> (stealCursor % static_cast<uint32_t> (kMaxVoices));
    stealCursor++;
    return index;
}

} // namespace sampr