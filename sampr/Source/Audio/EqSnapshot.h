#pragma once

#include <array>
#include <memory>

#include "../Plugins/EqTypes.h"
#include "PatternSnapshot.h"

namespace sampr
{

struct EqSnapshot
{
    int numChannels = 0;
    std::array<ChannelEqState, kMaxPatternChannels> channels {};
};

using SharedEqSnapshot = std::shared_ptr<const EqSnapshot>;

} // namespace sampr