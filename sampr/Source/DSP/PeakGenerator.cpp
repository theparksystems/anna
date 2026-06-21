#include "PeakGenerator.h"

namespace sampr
{

WaveformPeaks PeakGenerator::generate (const juce::AudioBuffer<float>& buffer,
                                        int targetColumns)
{
    WaveformPeaks peaks;
    peaks.numColumns = juce::jmax (1, targetColumns);
    peaks.minMax.resize (static_cast<size_t> (peaks.numColumns * 2), 0.0f);

    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return peaks;

    const auto samplesPerColumn = juce::jmax (1, numSamples / peaks.numColumns);

    for (int column = 0; column < peaks.numColumns; ++column)
    {
        const auto start = column * samplesPerColumn;
        const auto end = juce::jmin (numSamples, start + samplesPerColumn);

        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (int sample = start; sample < end; ++sample)
        {
            float mixed = 0.0f;

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mixed += buffer.getSample (ch, sample);

            mixed /= static_cast<float> (juce::jmax (1, buffer.getNumChannels()));
            minVal = juce::jmin (minVal, mixed);
            maxVal = juce::jmax (maxVal, mixed);
        }

        const auto idx = static_cast<size_t> (column * 2);
        peaks.minMax[idx] = minVal;
        peaks.minMax[idx + 1] = maxVal;
    }

    return peaks;
}

} // namespace sampr