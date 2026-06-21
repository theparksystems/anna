#include "RubberBandOfflineProcessor.h"

#include "../Audio/AudioTypes.h"

#if SAMPR_HAS_RUBBERBAND
 #include <rubberband/RubberBandStretcher.h>
#endif

namespace sampr
{

bool RubberBandOfflineProcessor::isAvailable() noexcept
{
#if SAMPR_HAS_RUBBERBAND
    return true;
#else
    return false;
#endif
}

#if SAMPR_HAS_RUBBERBAND

namespace
{
    constexpr int kProcessBlockSize = 1024;

    void interleave (const juce::AudioBuffer<float>& input,
                     int startSample,
                     int numSamples,
                     std::vector<float>& interleaved)
    {
        const auto channels = static_cast<size_t> (input.getNumChannels());
        interleaved.resize (static_cast<size_t> (numSamples) * channels);

        for (int i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < channels; ++ch)
                interleaved[static_cast<size_t> (i) * channels + ch]
                    = input.getSample (static_cast<int> (ch), startSample + i);
        }
    }

    void deinterleave (const float* interleaved,
                       int numSamples,
                       int numChannels,
                       juce::AudioBuffer<float>& output,
                       int outputStart)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                output.setSample (ch,
                                  outputStart + i,
                                  interleaved[static_cast<size_t> (i) * static_cast<size_t> (numChannels)
                                              + static_cast<size_t> (ch)]);
        }
    }
}

std::unique_ptr<juce::AudioBuffer<float>> RubberBandOfflineProcessor::process (
    const juce::AudioBuffer<float>& input,
    double sampleRate,
    const StretchParameters& params)
{
    if (input.getNumSamples() <= 0 || input.getNumChannels() <= 0)
        return nullptr;

    const auto channels = static_cast<size_t> (input.getNumChannels());
    const auto options = RubberBand::RubberBandStretcher::OptionProcessOffline
                         | RubberBand::RubberBandStretcher::OptionEngineFiner
                         | RubberBand::RubberBandStretcher::OptionThreadingNever;

    RubberBand::RubberBandStretcher stretcher (static_cast<size_t> (sampleRate),
                                               channels,
                                               options,
                                               static_cast<double> (params.timeRatio),
                                               static_cast<double> (semitonesToRatio (params.pitchSemitones)));

    std::vector<const float*> inputChannelPtrs (channels);
    const auto totalSamples = input.getNumSamples();
    auto inputOffset = 0;

    while (inputOffset < totalSamples)
    {
        const auto block = juce::jmin (kProcessBlockSize, totalSamples - inputOffset);

        for (size_t ch = 0; ch < channels; ++ch)
            inputChannelPtrs[ch] = input.getReadPointer (static_cast<int> (ch)) + inputOffset;

        stretcher.study (inputChannelPtrs.data(), static_cast<size_t> (block), 1);
        inputOffset += block;
    }

    inputOffset = 0;

    while (inputOffset < totalSamples)
    {
        const auto block = juce::jmin (kProcessBlockSize, totalSamples - inputOffset);

        for (size_t ch = 0; ch < channels; ++ch)
            inputChannelPtrs[ch] = input.getReadPointer (static_cast<int> (ch)) + inputOffset;

        stretcher.process (inputChannelPtrs.data(), static_cast<size_t> (block), 1);
        inputOffset += block;
    }

    stretcher.process (nullptr, 0, 0);

    const auto expectedOutput = static_cast<int> (stretcher.getSamplesRequired());
    auto output = std::make_unique<juce::AudioBuffer<float>> (input.getNumChannels(),
                                                              juce::jmax (expectedOutput, totalSamples));
    output->clear();

    auto writePosition = 0;

    while (true)
    {
        const auto available = stretcher.available();
        if (available <= 0)
            break;

        const auto toRetrieve = static_cast<int> (available);
        std::vector<float*> outputChannelPtrs (channels);

        if (writePosition + toRetrieve > output->getNumSamples())
            output->setSize (input.getNumChannels(), writePosition + toRetrieve, true, true, true);

        for (size_t ch = 0; ch < channels; ++ch)
            outputChannelPtrs[ch] = output->getWritePointer (static_cast<int> (ch)) + writePosition;

        const auto retrieved = static_cast<int> (stretcher.retrieve (outputChannelPtrs.data(),
                                                                     static_cast<size_t> (toRetrieve)));
        if (retrieved <= 0)
            break;
        writePosition += retrieved;
    }

    output->setSize (input.getNumChannels(), writePosition, true, true, true);
    return output;
}

#endif

} // namespace sampr