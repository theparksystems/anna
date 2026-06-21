#pragma once

#include <atomic>
#include <memory>

#include "AudioDiagnostics.h"
#include "PatternSnapshot.h"
#include "SampleRegistry.h"
#include "VoicePool.h"

namespace sampr
{

// Audio-thread step sequencer. Reads immutable PatternSnapshot; triggers voices.
class PatternScheduler
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSnapshot (SharedPatternSnapshot snapshot) noexcept;
    void setEnabled (bool shouldEnable) noexcept;
    bool isEnabled() const noexcept;

    void setMasterGain (float gain) noexcept;

    void process (uint64_t blockStartSamplePosition,
                  int numSamples,
                  bool transportPlaying,
                  VoicePool& voicePool,
                  const SampleRegistry& registry,
                  AudioDiagnostics& diagnostics) noexcept;

    int getCurrentStepIndex() const noexcept;
    uint64_t getLoopCycle() const noexcept;

private:
    void triggerStep (int stepIndex,
                      uint64_t loopCycle,
                      VoicePool& voicePool,
                      const SampleRegistry& registry,
                      AudioDiagnostics& diagnostics) noexcept;

    bool passesProbability (float probability, int channel, int step, uint64_t loopCycle) const noexcept;

    std::atomic<SharedPatternSnapshot> snapshot;
    std::atomic<bool> enabled { true };
    std::atomic<float> masterGain { 1.0f };
    std::atomic<int> currentStepIndex { -1 };

    double deviceSampleRate = 44100.0;
    int lastTriggeredStep = -1;
    uint64_t lastLoopCycle = 0;
    uint64_t loopCycleCounter = 0;
};

} // namespace sampr