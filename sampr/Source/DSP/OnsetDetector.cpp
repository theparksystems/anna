#include "OnsetDetector.h"

#include <cmath>

namespace sampr
{

namespace
{
    void buildMonoEnvelope (const juce::AudioBuffer<float>& buffer,
                            int windowSamples,
                            std::vector<float>& envelope)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto numChannels = buffer.getNumChannels();
        envelope.assign (static_cast<size_t> (numSamples), 0.0f);

        if (numSamples <= 0)
            return;

        windowSamples = juce::jmax (1, windowSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
                sum += std::abs (buffer.getSample (ch, i));

            envelope[static_cast<size_t> (i)] = sum / static_cast<float> (numChannels);
        }

        std::vector<float> smoothed (envelope.size(), 0.0f);
        const auto halfWindow = windowSamples / 2;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto start = juce::jmax (0, i - halfWindow);
            const auto end = juce::jmin (numSamples, i + halfWindow + 1);
            float total = 0.0f;

            for (int j = start; j < end; ++j)
                total += envelope[static_cast<size_t> (j)];

            smoothed[static_cast<size_t> (i)] = total / static_cast<float> (end - start);
        }

        envelope.swap (smoothed);
    }
}

std::vector<int> OnsetDetector::detect (const juce::AudioBuffer<float>& buffer,
                                        double sampleRate,
                                        const OnsetDetectorOptions& options)
{
    std::vector<int> onsets;

    if (buffer.getNumSamples() < 256 || sampleRate <= 0.0)
        return onsets;

    const auto windowSamples = juce::jmax (64, static_cast<int> (sampleRate * 0.01));
    std::vector<float> envelope;
    buildMonoEnvelope (buffer, windowSamples, envelope);

    const auto minGapSamples = juce::jmax (1, static_cast<int> (options.minGapSeconds * sampleRate));
    const auto sensitivity = juce::jlimit (0.25f, 4.0f, options.sensitivity);

    std::vector<float> flux (envelope.size(), 0.0f);
    float fluxMean = 0.0f;

    for (size_t i = 1; i < envelope.size(); ++i)
    {
        flux[i] = juce::jmax (0.0f, envelope[i] - envelope[i - 1]);
        fluxMean += flux[i];
    }

    fluxMean /= static_cast<float> (juce::jmax (size_t (1), flux.size() - 1));

    float fluxVariance = 0.0f;

    for (size_t i = 1; i < flux.size(); ++i)
    {
        const auto delta = flux[i] - fluxMean;
        fluxVariance += delta * delta;
    }

    fluxVariance /= static_cast<float> (juce::jmax (size_t (1), flux.size() - 1));
    const auto fluxThreshold = fluxMean + std::sqrt (fluxVariance) * (0.8f / sensitivity);

    int lastOnset = -minGapSamples;

    for (size_t i = 2; i + 1 < flux.size(); ++i)
    {
        const auto curr = flux[i];

        if (curr <= fluxThreshold || curr <= flux[i - 1] || curr <= flux[i + 1])
            continue;

        const auto sampleIndex = static_cast<int> (i);

        if (sampleIndex - lastOnset < minGapSamples)
            continue;

        onsets.push_back (sampleIndex);
        lastOnset = sampleIndex;

        if (static_cast<int> (onsets.size()) >= options.maxSlices - 1)
            break;
    }

    return onsets;
}

} // namespace sampr