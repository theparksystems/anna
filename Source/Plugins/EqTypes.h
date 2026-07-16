#pragma once

#include <cstdint>

namespace sampr
{

enum class EqBandKind : uint8_t
{
    lowShelf = 0,
    peak,
    highShelf
};

struct EqBandState
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
};

struct ChannelEqState
{
    bool enabled = true;
    EqBandState low { 120.0f, 0.0f, 0.707f };
    EqBandState mid { 1000.0f, 0.0f, 1.0f };
    EqBandState high { 8000.0f, 0.0f, 0.707f };

    bool operator== (const ChannelEqState& other) const noexcept
    {
        return enabled == other.enabled
               && low.frequencyHz == other.low.frequencyHz
               && low.gainDb == other.low.gainDb
               && low.q == other.low.q
               && mid.frequencyHz == other.mid.frequencyHz
               && mid.gainDb == other.mid.gainDb
               && mid.q == other.mid.q
               && high.frequencyHz == other.high.frequencyHz
               && high.gainDb == other.high.gainDb
               && high.q == other.high.q;
    }

    bool operator!= (const ChannelEqState& other) const noexcept
    {
        return ! (*this == other);
    }
};

inline ChannelEqState makeDefaultChannelEq()
{
    return {};
}

} // namespace sampr