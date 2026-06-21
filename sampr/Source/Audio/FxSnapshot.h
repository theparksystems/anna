#pragma once

#include <array>
#include <memory>

#include "../Plugins/FxTypes.h"
#include "PatternSnapshot.h"

namespace sampr
{

struct FxSnapshot
{
    int numChannels = 0;
    std::array<ChannelFxState, kMaxPatternChannels> channels {};
};

using SharedFxSnapshot = std::shared_ptr<const FxSnapshot>;

} // namespace sampr