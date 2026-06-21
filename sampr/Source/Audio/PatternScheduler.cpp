#include "PatternScheduler.h"

namespace sampr
{

void PatternScheduler::prepare (double sampleRate) noexcept
{
    deviceSampleRate = sampleRate;
    reset();
}

void PatternScheduler::reset() noexcept
{
    lastTriggeredStep = -1;
    lastLoopCycle = 0;
    loopCycleCounter = 0;
    currentStepIndex.store (-1, std::memory_order_relaxed);
}

void PatternScheduler::setSnapshot (SharedPatternSnapshot newSnapshot) noexcept
{
    snapshot.store (std::move (newSnapshot), std::memory_order_release);
}

void PatternScheduler::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool PatternScheduler::isEnabled() const noexcept
{
    return enabled.load (std::memory_order_relaxed);
}

void PatternScheduler::setMasterGain (float gain) noexcept
{
    masterGain.store (juce::jlimit (0.0f, 2.0f, gain), std::memory_order_relaxed);
}

int PatternScheduler::getCurrentStepIndex() const noexcept
{
    return currentStepIndex.load (std::memory_order_relaxed);
}

uint64_t PatternScheduler::getLoopCycle() const noexcept
{
    return loopCycleCounter;
}

bool PatternScheduler::passesProbability (float probability,
                                          int channel,
                                          int step,
                                          uint64_t loopCycle) const noexcept
{
    if (probability >= 1.0f)
        return true;

    if (probability <= 0.0f)
        return false;

    auto hash = static_cast<uint32_t> (channel * 374761393u
                                       + step * 668265263u
                                       + static_cast<uint32_t> (loopCycle & 0xffffffffu) * 2147483647u);
    hash ^= hash >> 13;
    return static_cast<float> (hash % 1000u) < probability * 1000.0f;
}

void PatternScheduler::triggerStep (int stepIndex,
                                    uint64_t loopCycle,
                                    VoicePool& voicePool,
                                    const SampleRegistry& registry,
                                    AudioDiagnostics& diagnostics) noexcept
{
    const auto snap = snapshot.load (std::memory_order_acquire);

    if (snap == nullptr || snap->numSteps <= 0 || snap->samplesPerStep <= 0.0)
        return;

    const auto master = masterGain.load (std::memory_order_relaxed);

    for (int ch = 0; ch < snap->numChannels; ++ch)
    {
        const auto& channel = snap->channels[static_cast<size_t> (ch)];

        if (channel.sampleId == kInvalidSampleId)
            continue;

        if (channel.channelMute)
            continue;

        if (snap->anyChannelSolo && ! channel.channelSolo)
            continue;

        if (stepIndex < 0 || stepIndex >= snap->numSteps)
            continue;

        const auto& step = channel.steps[static_cast<size_t> (stepIndex)];

        if (! step.active)
            continue;

        if (! passesProbability (step.probability, ch, stepIndex, loopCycle))
            continue;

        voicePool.triggerSample (registry,
                                 channel.sampleId,
                                 step.velocity,
                                 master * channel.channelGain,
                                 channel.channelPan,
                                 channel.pitchSemitones,
                                 channel.timeRatio,
                                 channel.loop,
                                 channel.mode,
                                 ch,
                                 diagnostics);
    }
}

void PatternScheduler::process (uint64_t blockStartSamplePosition,
                                int numSamples,
                                bool transportPlaying,
                                VoicePool& voicePool,
                                const SampleRegistry& registry,
                                AudioDiagnostics& diagnostics) noexcept
{
    juce::ignoreUnused (deviceSampleRate);

    if (! transportPlaying || ! enabled.load (std::memory_order_relaxed))
    {
        currentStepIndex.store (-1, std::memory_order_relaxed);
        return;
    }

    const auto snap = snapshot.load (std::memory_order_acquire);

    if (snap == nullptr || snap->numSteps <= 0 || snap->samplesPerStep <= 0.0)
        return;

    const auto patternLengthSamples = static_cast<uint64_t> (
        snap->samplesPerStep * static_cast<double> (snap->numSteps));

    if (patternLengthSamples == 0)
        return;

    const auto blockEnd = blockStartSamplePosition + static_cast<uint64_t> (numSamples);

    for (uint64_t samplePos = blockStartSamplePosition; samplePos < blockEnd; ++samplePos)
    {
        const auto positionInPattern = samplePos % patternLengthSamples;
        const auto stepIndex = static_cast<int> (
            static_cast<double> (positionInPattern) / snap->samplesPerStep);
        const auto loopCycle = samplePos / patternLengthSamples;
        const auto stepStartInPattern = static_cast<uint64_t> (
            static_cast<double> (stepIndex) * snap->samplesPerStep);

        if (positionInPattern != stepStartInPattern)
            continue;

        if (stepIndex == lastTriggeredStep && loopCycle == loopCycleCounter)
            continue;

        triggerStep (stepIndex, loopCycle, voicePool, registry, diagnostics);
        lastTriggeredStep = stepIndex;
        loopCycleCounter = loopCycle;
        lastLoopCycle = loopCycle;
        currentStepIndex.store (stepIndex, std::memory_order_relaxed);
    }
}

} // namespace sampr