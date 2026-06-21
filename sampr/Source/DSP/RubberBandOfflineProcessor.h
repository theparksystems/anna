#pragma once

#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

struct StretchParameters
{
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;
};

class RubberBandOfflineProcessor
{
public:
#if SAMPR_HAS_RUBBERBAND
    static std::unique_ptr<juce::AudioBuffer<float>> process (const juce::AudioBuffer<float>& input,
                                                              double sampleRate,
                                                              const StretchParameters& params);
#endif

    static bool isAvailable() noexcept;
};

} // namespace sampr