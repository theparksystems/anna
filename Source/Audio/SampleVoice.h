#pragma once

#include <atomic>
#include <memory>

#include "../DSP/RubberBandStreamProcessor.h"
#include "AudioTypes.h"

namespace sampr
{

enum class VoicePlaybackMode : uint8_t
{
    raw = 0,
    timeStretch,      // pre-baked Rubber Band buffer
    realTimeStretch,  // streaming Rubber Band stretcher per voice
};

struct VoiceRenderParams
{
    float gain = 1.0f;
    float pan = 0.0f;
    float pitchRatio = 1.0f;
    float timeRatio = 1.0f;
    float pitchSemitones = 0.0f;
    bool loop = false;
    VoicePlaybackMode mode = VoicePlaybackMode::raw;
    int channelIndex = -1;
};

// One polyphonic playback voice. Audio thread only.
class SampleVoice
{
public:
    void prepare (double deviceSampleRate, int maxBlockSize) noexcept;
    void trigger (SharedSampleData sampleData, const VoiceRenderParams& params) noexcept;
    void stop() noexcept;

    bool isActive() const noexcept { return active.load (std::memory_order_acquire); }

    void renderAdding (juce::AudioBuffer<float>& output,
                       int startSample,
                       int numSamples,
                       class LevelMeterBank* meters) noexcept;

    int getChannelIndex() const noexcept { return channelIndex; }

private:
    float readInterpolated (const juce::AudioBuffer<float>& buffer, int channel, double position) const noexcept;
    void renderRawAdding (juce::AudioBuffer<float>& output, int startSample, int numSamples, LevelMeterBank* meters) noexcept;
    void renderRealTimeStretchAdding (juce::AudioBuffer<float>& output, int startSample, int numSamples, LevelMeterBank* meters) noexcept;

    std::atomic<bool> active { false };
    SharedSampleData sample;
    VoiceRenderParams params;
    double playhead = 0.0;
    double deviceSampleRate = 44100.0;
    double sourceToDeviceRatio = 1.0;
    int channelIndex = -1;

    std::unique_ptr<RubberBandStreamProcessor> streamProcessor;
};

} // namespace sampr