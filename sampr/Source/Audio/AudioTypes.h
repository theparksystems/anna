#pragma once

#include <cmath>
#include <cstdint>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

using SampleId = uint32_t;
inline constexpr SampleId kInvalidSampleId = 0;

struct SampleData
{
    std::shared_ptr<const juce::AudioBuffer<float>> buffer;
    double sourceSampleRate = 44100.0;
};

using SharedSampleData = std::shared_ptr<const SampleData>;

inline float velocityToGain (float velocity) noexcept
{
    return juce::jlimit (0.0f, 1.0f, velocity);
}

inline float semitonesToRatio (float semitones) noexcept
{
    return std::pow (2.0f, semitones / 12.0f);
}

} // namespace sampr