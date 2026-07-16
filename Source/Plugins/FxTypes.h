#pragma once

#include "EqTypes.h"

namespace sampr
{

struct ChannelCompressorState
{
    bool enabled = false;
    float thresholdDb = -18.0f;
    float ratio = 4.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float makeupGainDb = 0.0f;

    bool operator== (const ChannelCompressorState& other) const noexcept
    {
        return enabled == other.enabled
               && thresholdDb == other.thresholdDb
               && ratio == other.ratio
               && attackMs == other.attackMs
               && releaseMs == other.releaseMs
               && makeupGainDb == other.makeupGainDb;
    }

    bool operator!= (const ChannelCompressorState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct ChannelDelayState
{
    bool enabled = false;
    float timeMs = 250.0f;
    float feedback = 0.35f;
    float mix = 0.25f;

    bool operator== (const ChannelDelayState& other) const noexcept
    {
        return enabled == other.enabled
               && timeMs == other.timeMs
               && feedback == other.feedback
               && mix == other.mix;
    }

    bool operator!= (const ChannelDelayState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct ChannelReverbState
{
    bool enabled = false;
    float roomSize = 0.5f;
    float damping = 0.5f;
    float wetLevel = 0.25f;
    float dryLevel = 0.8f;
    float width = 1.0f;

    bool operator== (const ChannelReverbState& other) const noexcept
    {
        return enabled == other.enabled
               && roomSize == other.roomSize
               && damping == other.damping
               && wetLevel == other.wetLevel
               && dryLevel == other.dryLevel
               && width == other.width;
    }

    bool operator!= (const ChannelReverbState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct ChannelColorState
{
    bool enabled = false;
    float lowCutHz = 20.0f;
    float highCutHz = 20000.0f;
    float drive = 0.0f;
    float width = 1.0f;
    float chorusMix = 0.0f;
    float chorusRateHz = 0.35f;
    float phaserMix = 0.0f;
    float phaserRateHz = 0.25f;
    float pump = 0.0f;
    float limiterCeilingDb = -1.0f;

    bool operator== (const ChannelColorState& other) const noexcept
    {
        return enabled == other.enabled
               && lowCutHz == other.lowCutHz
               && highCutHz == other.highCutHz
               && drive == other.drive
               && width == other.width
               && chorusMix == other.chorusMix
               && chorusRateHz == other.chorusRateHz
               && phaserMix == other.phaserMix
               && phaserRateHz == other.phaserRateHz
               && pump == other.pump
               && limiterCeilingDb == other.limiterCeilingDb;
    }

    bool operator!= (const ChannelColorState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct ChannelFxState
{
    ChannelEqState eq;
    ChannelColorState color;
    ChannelCompressorState compressor;
    ChannelDelayState delay;
    ChannelReverbState reverb;

    bool operator== (const ChannelFxState& other) const noexcept
    {
        return eq == other.eq
               && color == other.color
               && compressor == other.compressor
               && delay == other.delay
               && reverb == other.reverb;
    }

    bool operator!= (const ChannelFxState& other) const noexcept
    {
        return ! (*this == other);
    }
};

inline ChannelFxState makeDefaultChannelFx()
{
    return {};
}

} // namespace sampr
