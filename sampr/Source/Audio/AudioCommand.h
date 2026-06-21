#pragma once

#include "AudioTypes.h"
#include "SampleVoice.h"

namespace sampr
{

enum class AudioCommandType : uint8_t
{
    none = 0,
    triggerVoice,
    stopAllVoices,
    setTransportPlaying,
    transportPlay,
    transportPause,
    transportStop,
    setBpm,
    setMasterGain,
    setTestToneEnabled,
    setMetronomeEnabled,
    setLoopEnabled,
    setLoopStartBeats,
    setLoopEndBeats,
};

struct AudioCommand
{
    AudioCommandType type = AudioCommandType::none;

    SampleId sampleId = kInvalidSampleId;
    int channelIndex = -1;
    float velocity = 1.0f;
    float gain = 1.0f;
    float pan = 0.0f;
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;

    bool boolValue = false;
    bool loop = false;
    float floatValue = 0.0f;
    VoicePlaybackMode playbackMode = VoicePlaybackMode::raw;
};

} // namespace sampr