#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

struct OnsetDetectorOptions
{
    float sensitivity = 1.0f;
    double minGapSeconds = 0.04;
    int maxSlices = 64;
};

class OnsetDetector
{
public:
    static std::vector<int> detect (const juce::AudioBuffer<float>& buffer,
                                    double sampleRate,
                                    const OnsetDetectorOptions& options = {});
};

} // namespace sampr