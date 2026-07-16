#pragma once

#include <array>

#include <juce_dsp/juce_dsp.h>

#include "EqTypes.h"

namespace sampr
{

class ParametricEqProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const ChannelEqState& state) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    struct Band
    {
        EqBandKind kind = EqBandKind::peak;
        float frequencyHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        juce::dsp::IIR::Filter<float> filterL;
        juce::dsp::IIR::Filter<float> filterR;
    };

    void updateCoefficients() noexcept;

    juce::dsp::ProcessSpec processSpec;
    std::array<Band, 3> bands;
    bool enabled = true;
    bool prepared = false;
};

} // namespace sampr