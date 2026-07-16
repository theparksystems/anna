#include "ColorProcessor.h"

#include <cmath>

namespace sampr
{

void ColorProcessor::prepare (double sampleRateIn, int maxBlockSize)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 44100.0;
    prepared = true;
    pumpPhase = 0.0f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (juce::jmax (1, maxBlockSize));
    spec.numChannels = 2;

    highPassL.prepare (spec);
    highPassR.prepare (spec);
    lowPassL.prepare (spec);
    lowPassR.prepare (spec);
    chorus.prepare (spec);
    phaser.prepare (spec);

    updateFilters();
    reset();
}

void ColorProcessor::reset() noexcept
{
    highPassL.reset();
    highPassR.reset();
    lowPassL.reset();
    lowPassR.reset();
    chorus.reset();
    phaser.reset();
    pumpPhase = 0.0f;
}

void ColorProcessor::setParameters (const ChannelColorState& nextState) noexcept
{
    state = nextState;
    state.lowCutHz = juce::jlimit (20.0f, 1000.0f, state.lowCutHz);
    state.highCutHz = juce::jlimit (1000.0f, 20000.0f, state.highCutHz);
    state.drive = juce::jlimit (0.0f, 1.0f, state.drive);
    state.width = juce::jlimit (0.0f, 2.0f, state.width);
    state.chorusMix = juce::jlimit (0.0f, 1.0f, state.chorusMix);
    state.chorusRateHz = juce::jlimit (0.05f, 5.0f, state.chorusRateHz);
    state.phaserMix = juce::jlimit (0.0f, 1.0f, state.phaserMix);
    state.phaserRateHz = juce::jlimit (0.05f, 5.0f, state.phaserRateHz);
    state.pump = juce::jlimit (0.0f, 1.0f, state.pump);
    state.limiterCeilingDb = juce::jlimit (-12.0f, 0.0f, state.limiterCeilingDb);

    chorus.setRate (state.chorusRateHz);
    chorus.setDepth (0.45f);
    chorus.setCentreDelay (7.0f);
    chorus.setFeedback (0.08f);
    chorus.setMix (state.chorusMix);

    phaser.setRate (state.phaserRateHz);
    phaser.setDepth (0.65f);
    phaser.setCentreFrequency (900.0f);
    phaser.setFeedback (0.18f);
    phaser.setMix (state.phaserMix);

    updateFilters();
}

void ColorProcessor::updateFilters() noexcept
{
    if (! prepared)
        return;

    *highPassL.coefficients = *Coefficients::makeHighPass (sampleRate, state.lowCutHz);
    *highPassR.coefficients = *Coefficients::makeHighPass (sampleRate, state.lowCutHz);
    *lowPassL.coefficients = *Coefficients::makeLowPass (sampleRate, state.highCutHz);
    *lowPassR.coefficients = *Coefficients::makeLowPass (sampleRate, state.highCutHz);
}

void ColorProcessor::process (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    if (! state.enabled || ! prepared || numSamples <= 0)
        return;

    auto* left = buffer.getWritePointer (0, startSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : nullptr;
    const auto driveGain = 1.0f + state.drive * 14.0f;
    const auto driveTrim = 1.0f / (1.0f + state.drive * 2.8f);
    const auto ceiling = juce::Decibels::decibelsToGain (state.limiterCeilingDb);
    const auto pumpDepth = state.pump * 0.55f;
    const auto pumpStep = static_cast<float> (2.0 * juce::MathConstants<double>::pi * 2.0 / sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        auto l = highPassL.processSample (left[i]);
        l = lowPassL.processSample (l);
        auto r = right != nullptr ? highPassR.processSample (right[i]) : l;
        r = lowPassR.processSample (r);

        if (state.drive > 0.0f)
        {
            l = std::tanh (l * driveGain) * driveTrim;
            r = std::tanh (r * driveGain) * driveTrim;
        }

        if (state.width != 1.0f && right != nullptr)
        {
            const auto mid = (l + r) * 0.5f;
            const auto side = (l - r) * 0.5f * state.width;
            l = mid + side;
            r = mid - side;
        }

        if (pumpDepth > 0.0f)
        {
            const auto duck = 1.0f - pumpDepth * (0.5f + 0.5f * std::sin (pumpPhase));
            pumpPhase += pumpStep;
            if (pumpPhase > juce::MathConstants<float>::twoPi)
                pumpPhase -= juce::MathConstants<float>::twoPi;
            l *= duck;
            r *= duck;
        }

        left[i] = juce::jlimit (-ceiling, ceiling, l);
        if (right != nullptr)
            right[i] = juce::jlimit (-ceiling, ceiling, r);
    }

    if (state.chorusMix > 0.0f || state.phaserMix > 0.0f)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto subBlock = block.getSubBlock (static_cast<size_t> (startSample), static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextReplacing<float> context (subBlock);

        if (state.chorusMix > 0.0f)
            chorus.process (context);

        if (state.phaserMix > 0.0f)
            phaser.process (context);
    }
}

} // namespace sampr
