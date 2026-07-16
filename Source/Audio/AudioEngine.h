#pragma once

#include <array>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "AudioCommandQueue.h"
#include "AudioDiagnostics.h"
#include "ChannelMixer.h"
#include "FxSnapshot.h"
#include "LevelMeterBank.h"
#include "Mixer.h"
#include "NoteScheduler.h"
#include "NoteSnapshot.h"
#include "PatternScheduler.h"
#include "PatternSnapshot.h"
#include "SampleRegistry.h"
#include "Transport.h"
#include "VoicePool.h"

namespace sampr
{

// Real-time audio engine. UI thread pushes commands; audio thread renders.
class AudioEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void release();

    SampleId registerSample (SharedSampleData sampleData) noexcept;
    void unregisterSample (SampleId sampleId) noexcept;

    bool pushCommand (const AudioCommand& command) noexcept;
    uint32_t getDroppedCommandCount() const noexcept;

    void render (juce::AudioBuffer<float>& buffer,
                 int startSample,
                 int numSamples) noexcept;

    void renderOffline (juce::AudioBuffer<float>& buffer,
                        int startSample,
                        int numSamples) noexcept;

    bool isTransportPlaying() const noexcept;
    bool isTransportPaused() const noexcept;
    double getBpm() const noexcept;
    int getActiveVoiceCount() const noexcept;
    uint64_t getTransportSamplePosition() const noexcept;
    double getPlayheadBeats (double bpm) const noexcept;

    bool isLoopEnabled() const noexcept;
    double getLoopStartBeats() const noexcept;
    double getLoopEndBeats() const noexcept;
    bool isMetronomeEnabled() const noexcept;

    void setPatternSnapshot (SharedPatternSnapshot snapshot) noexcept;
    void setNoteSnapshot (SharedNoteSnapshot snapshot) noexcept;
    void setFxSnapshot (SharedFxSnapshot snapshot) noexcept;
    void setSequencerEnabled (bool enabled) noexcept;
    void setNoteSequencerEnabled (bool enabled) noexcept;
    int getCurrentSequencerStep() const noexcept;

    LevelMeterBank& getLevelMeters() noexcept { return levelMeters; }
    const LevelMeterBank& getLevelMeters() const noexcept { return levelMeters; }

    AudioDiagnostics& getDiagnostics() noexcept { return diagnostics; }
    const AudioDiagnostics& getDiagnostics() const noexcept { return diagnostics; }

    Transport& getTransport() noexcept { return transport; }
    const Transport& getTransport() const noexcept { return transport; }

    void setOfflineRenderActive (bool active) noexcept;
    bool isOfflineRenderActive() const noexcept;

private:
    void processCommands (int maxCommands) noexcept;
    void renderTestTone (juce::AudioBuffer<float>& buffer,
                         int startSample,
                         int numSamples) noexcept;
    void renderMetronome (juce::AudioBuffer<float>& buffer,
                          int startSample,
                          int numSamples,
                          uint64_t blockStartPosition) noexcept;

    double currentSampleRate = 44100.0;
    std::atomic<bool> testToneEnabled { false };
    std::atomic<bool> metronomeEnabled { false };
    std::atomic<double> tonePhase { 0.0 };
    std::atomic<double> metronomePhase { 0.0 };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<bool> offlineRenderActive { false };

    AudioCommandQueue commandQueue;
    AudioDiagnostics diagnostics;
    LevelMeterBank levelMeters;
    SampleRegistry sampleRegistry;
    VoicePool voicePool;
    ChannelMixer channelMixer;
    Mixer mixer;
    Transport transport;
    PatternScheduler patternScheduler;
    NoteScheduler noteScheduler;

    std::array<AudioCommand, 64> commandScratch {};
};

} // namespace sampr