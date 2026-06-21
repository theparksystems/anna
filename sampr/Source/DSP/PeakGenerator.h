#pragma once

#include "../Model/SampleAsset.h"

namespace sampr
{

class PeakGenerator
{
public:
    static WaveformPeaks generate (const juce::AudioBuffer<float>& buffer,
                                   int targetColumns = 2048);
};

} // namespace sampr