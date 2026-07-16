#include "ParametricEqProcessor.h"

namespace sampr
{

namespace
{
    using Coefficients = juce::dsp::IIR::Coefficients<float>;

    juce::ReferenceCountedObjectPtr<Coefficients> makeCoefficients (EqBandKind kind,
                                                                    double sampleRate,
                                                                    float frequencyHz,
                                                                    float q,
                                                                    float gainDb)
    {
        const auto gain = juce::Decibels::decibelsToGain (gainDb);
        const auto freq = juce::jlimit (20.0f, 20000.0f, frequencyHz);
        const auto bandwidth = juce::jmax (0.1f, q);

        switch (kind)
        {
            case EqBandKind::lowShelf:
                return Coefficients::makeLowShelf (sampleRate, freq, bandwidth, gain);
            case EqBandKind::highShelf:
                return Coefficients::makeHighShelf (sampleRate, freq, bandwidth, gain);
            case EqBandKind::peak:
            default:
                return Coefficients::makePeakFilter (sampleRate, freq, bandwidth, gain);
        }
    }
}

void ParametricEqProcessor::prepare (double sampleRate, int maxBlockSize)
{
    processSpec.sampleRate = sampleRate;
    processSpec.maximumBlockSize = static_cast<juce::uint32> (juce::jmax (1, maxBlockSize));
    processSpec.numChannels = 1;

    for (auto& band : bands)
    {
        band.filterL.prepare (processSpec);
        band.filterR.prepare (processSpec);
    }

    prepared = true;
    updateCoefficients();
}

void ParametricEqProcessor::reset() noexcept
{
    for (auto& band : bands)
    {
        band.filterL.reset();
        band.filterR.reset();
    }
}

void ParametricEqProcessor::setParameters (const ChannelEqState& state) noexcept
{
    enabled = state.enabled;

    bands[0].kind = EqBandKind::lowShelf;
    bands[0].frequencyHz = state.low.frequencyHz;
    bands[0].gainDb = state.low.gainDb;
    bands[0].q = state.low.q;

    bands[1].kind = EqBandKind::peak;
    bands[1].frequencyHz = state.mid.frequencyHz;
    bands[1].gainDb = state.mid.gainDb;
    bands[1].q = state.mid.q;

    bands[2].kind = EqBandKind::highShelf;
    bands[2].frequencyHz = state.high.frequencyHz;
    bands[2].gainDb = state.high.gainDb;
    bands[2].q = state.high.q;

    updateCoefficients();
}

void ParametricEqProcessor::updateCoefficients() noexcept
{
    if (! prepared)
        return;

    const auto sampleRate = processSpec.sampleRate;

    for (auto& band : bands)
    {
        const auto coefs = makeCoefficients (band.kind,
                                             sampleRate,
                                             band.frequencyHz,
                                             band.q,
                                             band.gainDb);
        *band.filterL.coefficients = *coefs;
        *band.filterR.coefficients = *coefs;
    }
}

void ParametricEqProcessor::process (juce::AudioBuffer<float>& buffer,
                                     int startSample,
                                     int numSamples) noexcept
{
    if (! enabled || ! prepared || numSamples <= 0)
        return;

    auto* left = buffer.getWritePointer (0, startSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : left;

    for (auto& band : bands)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = band.filterL.processSample (left[i]);

            if (right != left)
                right[i] = band.filterR.processSample (right[i]);
        }
    }
}

} // namespace sampr