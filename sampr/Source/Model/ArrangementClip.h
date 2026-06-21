#pragma once

#include <juce_graphics/juce_graphics.h>

#include "Pattern.h"

namespace sampr
{

using ClipId = uint32_t;
inline constexpr ClipId kInvalidClipId = 0;

enum class ClipType
{
    pattern,
};

struct ArrangementClip
{
    ClipId id = kInvalidClipId;
    ClipType type = ClipType::pattern;
    PatternId patternId = kInvalidPatternId;
    double startBar = 0.0;
    int lengthBars = 4;
    juce::String name;
    juce::Colour colour { 0xff4cc2ff };
};

} // namespace sampr