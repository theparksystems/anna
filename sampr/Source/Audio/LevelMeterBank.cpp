#include "LevelMeterBank.h"

#include <juce_core/juce_core.h>

namespace sampr
{

void LevelMeterBank::reset() noexcept
{
    for (auto& peak : peaks)
        peak.store (0.0f, std::memory_order_relaxed);
}

void LevelMeterBank::submitPeak (int channelIndex, float peak) noexcept
{
    if (! juce::isPositiveAndBelow (channelIndex, kMaxPatternChannels))
        return;

    const auto clamped = juce::jlimit (0.0f, 1.0f, peak);
    auto& slot = peaks[static_cast<size_t> (channelIndex)];
    auto current = slot.load (std::memory_order_relaxed);

    while (clamped > current && ! slot.compare_exchange_weak (current, clamped, std::memory_order_relaxed))
    {}
}

void LevelMeterBank::submitMasterPeak (float peak) noexcept
{
    submitPeak (kMasterMeterIndex, peak);
}

float LevelMeterBank::getPeak (int channelIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (channelIndex, kMaxPatternChannels))
        return 0.0f;

    return peaks[static_cast<size_t> (channelIndex)].load (std::memory_order_relaxed);
}

float LevelMeterBank::getMasterPeak() const noexcept
{
    return peaks[static_cast<size_t> (kMasterMeterIndex)].load (std::memory_order_relaxed);
}

void LevelMeterBank::decayPeaks (float factor) noexcept
{
    const auto decay = juce::jlimit (0.0f, 1.0f, factor);

    for (auto& peak : peaks)
    {
        const auto current = peak.load (std::memory_order_relaxed);
        peak.store (current * decay, std::memory_order_relaxed);
    }
}

} // namespace sampr