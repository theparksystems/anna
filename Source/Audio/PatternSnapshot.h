#pragma once

#include <array>
#include <memory>

#include "AudioTypes.h"
#include "SampleVoice.h"

namespace sampr
{

inline constexpr int kMaxPatternSteps = 64;
inline constexpr int kMaxPatternChannels = 16;

struct StepTriggerData
{
    bool active = false;
    float velocity = 1.0f;
    float probability = 1.0f;
};

struct ChannelTriggerData
{
    SampleId sampleId = kInvalidSampleId;
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;
    bool loop = false;
    VoicePlaybackMode mode = VoicePlaybackMode::raw;
    float channelGain = 1.0f;
    float channelPan = 0.0f;
    bool channelMute = false;
    bool channelSolo = false;
    std::array<StepTriggerData, kMaxPatternSteps> steps {};
};

struct PatternSnapshot
{
    int numSteps = 16;
    int numChannels = 0;
    double samplesPerStep = 0.0;
    bool anyChannelSolo = false;

    std::array<ChannelTriggerData, kMaxPatternChannels> channels {};
};

using SharedPatternSnapshot = std::shared_ptr<const PatternSnapshot>;

} // namespace sampr