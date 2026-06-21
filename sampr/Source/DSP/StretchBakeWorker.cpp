#include "StretchBakeWorker.h"

#include "RubberBandOfflineProcessor.h"

#include <juce_events/juce_events.h>

namespace sampr
{

StretchBakeWorker::StretchBakeWorker (ResultCallback callback)
    : juce::Thread ("SAMPR StretchBake"),
      onResult (std::move (callback))
{
    startThread (juce::Thread::Priority::normal);
}

StretchBakeWorker::~StretchBakeWorker()
{
    signalThreadShouldExit();
    jobSignal.signal();
    stopThread (5000);
}

void StretchBakeWorker::enqueue (StretchBakeJob job)
{
    {
        const std::scoped_lock lock (queueMutex);
        pendingJobs.push (std::move (job));
    }
    jobSignal.signal();
}

void StretchBakeWorker::clearPending()
{
    const std::scoped_lock lock (queueMutex);
    std::queue<StretchBakeJob> empty;
    std::swap (pendingJobs, empty);
}

void StretchBakeWorker::run()
{
    while (! threadShouldExit())
    {
        jobSignal.wait (100);

        std::optional<StretchBakeJob> job;

        {
            const std::scoped_lock lock (queueMutex);
            if (! pendingJobs.empty())
            {
                job = std::move (pendingJobs.front());
                pendingJobs.pop();
            }
        }

        if (! job.has_value())
            continue;

        StretchBakeResult result;
        result.assetId = job->assetId;
        result.sliceId = job->sliceId;

        if (job->sourceSlice != nullptr && job->sourceSlice->buffer != nullptr)
        {
#if SAMPR_HAS_RUBBERBAND
            result.bakedBuffer = RubberBandOfflineProcessor::process (*job->sourceSlice->buffer,
                                                                      job->sourceSlice->sourceSampleRate,
                                                                      job->params);
            result.success = result.bakedBuffer != nullptr && result.bakedBuffer->getNumSamples() > 0;
#else
            juce::ignoreUnused (job);
            result.success = false;
#endif
        }

        if (onResult != nullptr)
        {
            juce::MessageManager::callAsync ([cb = onResult, result]
            {
                cb (result);
            });
        }
    }
}

} // namespace sampr