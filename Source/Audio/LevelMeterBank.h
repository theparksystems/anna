#pragma once

#include <array>
#include <atomic>

#include "PatternSnapshot.h"

namespace sampr
{

inline constexpr int kMasterMeterIndex = kMaxPatternChannels;

class LevelMeterBank
{
public:
    void reset() noexcept;

    void submitPeak (int channelIndex, float peak) noexcept;
    void submitMasterPeak (float peak) noexcept;

    float getPeak (int channelIndex) const noexcept;
    float getMasterPeak() const noexcept;

    void decayPeaks (float factor) noexcept;

private:
    std::array<std::atomic<float>, kMaxPatternChannels + 1> peaks {};
};

} // namespace sampr