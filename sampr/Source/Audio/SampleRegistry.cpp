#include "SampleRegistry.h"

namespace sampr
{

namespace
{
    int slotIndexForId (SampleId sampleId) noexcept
    {
        if (sampleId == kInvalidSampleId)
            return -1;

        return static_cast<int> (sampleId - 1);
    }
}

SampleId SampleRegistry::registerSample (SharedSampleData sampleData) noexcept
{
    if (sampleData == nullptr || sampleData->buffer == nullptr)
        return kInvalidSampleId;

    const auto id = nextId.fetch_add (1, std::memory_order_relaxed);
    const auto slot = slotIndexForId (id);

    if (slot < 0 || slot >= kMaxSamples)
        return kInvalidSampleId;

    slots[static_cast<size_t> (slot)].store (std::move (sampleData), std::memory_order_release);
    registeredCount.fetch_add (1, std::memory_order_relaxed);
    return id;
}

void SampleRegistry::unregisterSample (SampleId sampleId) noexcept
{
    const auto slot = slotIndexForId (sampleId);

    if (slot < 0 || slot >= kMaxSamples)
        return;

    const auto previous = slots[static_cast<size_t> (slot)].exchange (nullptr, std::memory_order_acq_rel);

    if (previous != nullptr)
        registeredCount.fetch_sub (1, std::memory_order_relaxed);
}

SharedSampleData SampleRegistry::getSample (SampleId sampleId) const noexcept
{
    const auto slot = slotIndexForId (sampleId);

    if (slot < 0 || slot >= kMaxSamples)
        return nullptr;

    return slots[static_cast<size_t> (slot)].load (std::memory_order_acquire);
}

} // namespace sampr