#pragma once

#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

class RubberBandStreamProcessor
{
public:
    RubberBandStreamProcessor();
    ~RubberBandStreamProcessor();

    void begin (const juce::AudioBuffer<float>& source,
                double sampleRate,
                float timeRatio,
                float pitchSemitones);

    bool isActive() const noexcept { return active; }

    bool renderSample (float& left, float& right);

    void reset() noexcept;

    static bool isAvailable() noexcept;

private:
    struct Impl;
    void pumpOutput();

    std::unique_ptr<Impl> impl;
    bool active = false;
    bool sourceEnded = false;
    bool flushSent = false;
    int sourcePosition = 0;
    int sourceLength = 0;
    const juce::AudioBuffer<float>* sourceBuffer = nullptr;
    int numChannels = 0;

    std::vector<float> pendingLeft;
    std::vector<float> pendingRight;
    size_t pendingReadIndex = 0;
};

} // namespace sampr