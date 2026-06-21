#pragma once

#include <array>
#include <atomic>

#include "AudioTypes.h"

namespace sampr
{

// Immutable sample table. UI thread publishes; audio thread reads by SampleId.
class SampleRegistry
{
public:
    static constexpr int kMaxSamples = 128;

    SampleId registerSample (SharedSampleData sampleData) noexcept;
    void unregisterSample (SampleId sampleId) noexcept;

    SharedSampleData getSample (SampleId sampleId) const noexcept;

    int getRegisteredCount() const noexcept
    {
        return registeredCount.load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<SharedSampleData>, kMaxSamples> slots {};
    std::atomic<uint32_t> nextId { 1 };
    std::atomic<int> registeredCount { 0 };
};

} // namespace sampr