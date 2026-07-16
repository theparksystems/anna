#pragma once

#include <juce_dsp/juce_dsp.h>

#include "FxTypes.h"

namespace sampr
{

class ColorProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setParameters (const ChannelColorState& state) noexcept;
    void process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

private:
    void updateFilters() noexcept;

    ChannelColorState state;
    double sampleRate = 44100.0;
    bool prepared = false;
    float pumpPhase = 0.0f;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    Filter highPassL;
    Filter highPassR;
    Filter lowPassL;
    Filter lowPassR;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
};

} // namespace sampr
