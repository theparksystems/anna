#include "RubberBandStreamProcessor.h"

#include "../Audio/AudioTypes.h"

#if ANNA_HAS_RUBBERBAND
 #include <rubberband/RubberBandStretcher.h>
#endif

namespace sampr
{

#if ANNA_HAS_RUBBERBAND
struct RubberBandStreamProcessor::Impl
{
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
};
#else
struct RubberBandStreamProcessor::Impl {};
#endif

bool RubberBandStreamProcessor::isAvailable() noexcept
{
#if ANNA_HAS_RUBBERBAND
    return true;
#else
    return false;
#endif
}

RubberBandStreamProcessor::RubberBandStreamProcessor()
    : impl (std::make_unique<Impl>())
{
}

RubberBandStreamProcessor::~RubberBandStreamProcessor() = default;

void RubberBandStreamProcessor::reset() noexcept
{
    active = false;
    sourceEnded = false;
    flushSent = false;
    sourcePosition = 0;
    sourceLength = 0;
    sourceBuffer = nullptr;
    numChannels = 0;
    pendingLeft.clear();
    pendingRight.clear();
    pendingReadIndex = 0;

#if ANNA_HAS_RUBBERBAND
    if (impl != nullptr)
        impl->stretcher.reset();
#endif
}

void RubberBandStreamProcessor::begin (const juce::AudioBuffer<float>& source,
                                       double sampleRate,
                                       float timeRatio,
                                       float pitchSemitones)
{
    reset();

#if ! ANNA_HAS_RUBBERBAND
    juce::ignoreUnused (source, sampleRate, timeRatio, pitchSemitones);
    return;
#else
    if (source.getNumSamples() <= 0 || source.getNumChannels() <= 0 || sampleRate <= 0.0)
        return;

    numChannels = source.getNumChannels();
    sourceBuffer = &source;
    sourceLength = source.getNumSamples();

    const auto options = RubberBand::RubberBandStretcher::OptionProcessRealTime
                         | RubberBand::RubberBandStretcher::OptionEngineFaster;

    impl->stretcher = std::make_unique<RubberBand::RubberBandStretcher> (
        static_cast<size_t> (sampleRate),
        static_cast<size_t> (numChannels),
        options,
        static_cast<double> (juce::jlimit (0.25f, 4.0f, timeRatio)),
        static_cast<double> (semitonesToRatio (pitchSemitones)));

    active = true;
    pumpOutput();
#endif
}

void RubberBandStreamProcessor::pumpOutput()
{
#if ANNA_HAS_RUBBERBAND
    if (! active || impl == nullptr || impl->stretcher == nullptr || sourceBuffer == nullptr)
        return;

    auto& stretcher = *impl->stretcher;

    while (pendingReadIndex >= pendingLeft.size())
    {
        if (! sourceEnded)
        {
            if (sourcePosition < sourceLength)
            {
                const auto required = static_cast<int> (stretcher.getSamplesRequired());
                const auto toFeed = juce::jmin (required, sourceLength - sourcePosition);
                std::vector<const float*> channelPtrs (static_cast<size_t> (numChannels));

                for (int ch = 0; ch < numChannels; ++ch)
                    channelPtrs[static_cast<size_t> (ch)]
                        = sourceBuffer->getReadPointer (ch) + sourcePosition;

                stretcher.process (channelPtrs.data(), static_cast<size_t> (toFeed), false);
                sourcePosition += toFeed;
            }
            else if (! flushSent)
            {
                stretcher.process (nullptr, 0, true);
                flushSent = true;
                sourceEnded = true;
            }
            else
            {
                sourceEnded = true;
            }
        }

        const auto available = stretcher.available();

        if (available <= 0)
        {
            if (sourceEnded && flushSent)
                break;

            continue;
        }

        const auto toRetrieve = static_cast<int> (available);
        const auto writeIndex = pendingLeft.size();
        pendingLeft.resize (writeIndex + static_cast<size_t> (toRetrieve));
        pendingRight.resize (writeIndex + static_cast<size_t> (toRetrieve));

        std::vector<float*> outputPtrs (static_cast<size_t> (numChannels));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch == 0)
                outputPtrs[0] = pendingLeft.data() + static_cast<ptrdiff_t> (writeIndex);
            else if (ch == 1)
                outputPtrs[1] = pendingRight.data() + static_cast<ptrdiff_t> (writeIndex);
            else
                outputPtrs[static_cast<size_t> (ch)] = pendingLeft.data() + static_cast<ptrdiff_t> (writeIndex);
        }

        const auto retrieved = static_cast<int> (
            stretcher.retrieve (outputPtrs.data(), static_cast<size_t> (toRetrieve)));

        if (retrieved <= 0)
            break;

        if (numChannels == 1)
        {
            for (int i = 0; i < retrieved; ++i)
                pendingRight[writeIndex + static_cast<size_t> (i)]
                    = pendingLeft[writeIndex + static_cast<size_t> (i)];
        }
    }
#endif
}

bool RubberBandStreamProcessor::renderSample (float& left, float& right)
{
    if (! active)
        return false;

    pumpOutput();

    if (pendingReadIndex >= pendingLeft.size())
    {
        active = false;
        return false;
    }

    left = pendingLeft[pendingReadIndex];
    right = pendingRight[pendingReadIndex];
    ++pendingReadIndex;
    return true;
}

} // namespace sampr
