#pragma once

#include <array>

#include "AudioDiagnostics.h"
#include "SampleRegistry.h"
#include "SampleVoice.h"

namespace sampr
{

class VoicePool
{
public:
    static constexpr int kMaxVoices = 32;

    void prepare (double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    void triggerSample (const SampleRegistry& registry,
                        SampleId sampleId,
                        float velocity,
                        float gain,
                        float pan,
                        float pitchSemitones,
                        float timeRatio,
                        bool loop,
                        VoicePlaybackMode mode,
                        int channelIndex,
                        AudioDiagnostics& diagnostics) noexcept;

    void stopAll() noexcept;
    int getActiveVoiceCount() const noexcept;

    void renderAdding (juce::AudioBuffer<float>& masterOutput,
                       class ChannelMixer* channelMixer,
                       int startSample,
                       int numSamples,
                       class LevelMeterBank* meters) noexcept;

private:
    int findFreeVoice() const noexcept;
    int findVoiceToSteal() noexcept;

    std::array<SampleVoice, kMaxVoices> voices {};
    uint32_t stealCursor = 0;
};

} // namespace sampr