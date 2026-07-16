#include "AudioEngine.h"

#include <cmath>

namespace sampr
{

namespace
{
    constexpr double kTestToneFrequencyHz = 440.0;
    constexpr float kTestToneGain = 0.12f;
    constexpr float kMetronomeGain = 0.18f;
    constexpr double kMetronomeClickMs = 8.0;
}

void AudioEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    tonePhase.store (0.0, std::memory_order_relaxed);
    metronomePhase.store (0.0, std::memory_order_relaxed);
    levelMeters.reset();

    transport.prepare (sampleRate);
    voicePool.prepare (sampleRate, maxBlockSize);
    channelMixer.prepare (sampleRate, maxBlockSize);
    patternScheduler.prepare (sampleRate);
    noteScheduler.prepare (sampleRate);
}

void AudioEngine::release()
{
    pushCommand (AudioCommand { .type = AudioCommandType::stopAllVoices });
    pushCommand (AudioCommand { .type = AudioCommandType::transportStop });
    transport.reset();
    voicePool.reset();
    patternScheduler.reset();
    noteScheduler.reset();
    levelMeters.reset();
}

SampleId AudioEngine::registerSample (SharedSampleData sampleData) noexcept
{
    return sampleRegistry.registerSample (std::move (sampleData));
}

void AudioEngine::unregisterSample (SampleId sampleId) noexcept
{
    sampleRegistry.unregisterSample (sampleId);
}

bool AudioEngine::pushCommand (const AudioCommand& command) noexcept
{
    return commandQueue.push (command);
}

uint32_t AudioEngine::getDroppedCommandCount() const noexcept
{
    return commandQueue.getDroppedCount();
}

bool AudioEngine::isTransportPlaying() const noexcept
{
    return transport.isPlaying();
}

bool AudioEngine::isTransportPaused() const noexcept
{
    return ! transport.isPlaying() && transport.getSamplePosition() > 0;
}

double AudioEngine::getBpm() const noexcept
{
    return transport.getBpm();
}

int AudioEngine::getActiveVoiceCount() const noexcept
{
    return activeVoiceCount.load (std::memory_order_relaxed);
}

uint64_t AudioEngine::getTransportSamplePosition() const noexcept
{
    return transport.getSamplePosition();
}

double AudioEngine::getPlayheadBeats (double bpm) const noexcept
{
    return noteScheduler.getPlayheadBeats (transport.getSamplePosition(), bpm, currentSampleRate);
}

bool AudioEngine::isLoopEnabled() const noexcept
{
    return transport.isLoopEnabled();
}

double AudioEngine::getLoopStartBeats() const noexcept
{
    return transport.getLoopStartBeats();
}

double AudioEngine::getLoopEndBeats() const noexcept
{
    return transport.getLoopEndBeats();
}

bool AudioEngine::isMetronomeEnabled() const noexcept
{
    return metronomeEnabled.load (std::memory_order_relaxed);
}

void AudioEngine::setPatternSnapshot (SharedPatternSnapshot snapshot) noexcept
{
    patternScheduler.setSnapshot (std::move (snapshot));
}

void AudioEngine::setSequencerEnabled (bool enabled) noexcept
{
    patternScheduler.setEnabled (enabled);
}

void AudioEngine::setNoteSnapshot (SharedNoteSnapshot snapshot) noexcept
{
    noteScheduler.setSnapshot (std::move (snapshot));
}

void AudioEngine::setFxSnapshot (SharedFxSnapshot snapshot) noexcept
{
    channelMixer.setFxSnapshot (std::move (snapshot));
}

void AudioEngine::setNoteSequencerEnabled (bool enabled) noexcept
{
    noteScheduler.setEnabled (enabled);
}

int AudioEngine::getCurrentSequencerStep() const noexcept
{
    return patternScheduler.getCurrentStepIndex();
}

void AudioEngine::processCommands (int maxCommands) noexcept
{
    const auto numCommands = commandQueue.drain (commandScratch.data(), maxCommands);

    for (int i = 0; i < numCommands; ++i)
    {
        const auto& command = commandScratch[static_cast<size_t> (i)];

        switch (command.type)
        {
            case AudioCommandType::triggerVoice:
            {
                voicePool.triggerSample (sampleRegistry,
                                         command.sampleId,
                                         command.velocity,
                                         command.gain,
                                         command.pan,
                                         command.pitchSemitones,
                                         command.timeRatio,
                                         command.loop,
                                         command.playbackMode,
                                         command.channelIndex,
                                         diagnostics);
                break;
            }

            case AudioCommandType::stopAllVoices:
                voicePool.stopAll();
                break;

            case AudioCommandType::setTransportPlaying:
                transport.setPlaying (command.boolValue);

                if (command.boolValue)
                {
                    patternScheduler.reset();
                    noteScheduler.reset();
                }

                break;

            case AudioCommandType::transportPlay:
                transport.play();
                patternScheduler.reset();
                noteScheduler.reset();
                break;

            case AudioCommandType::transportPause:
                transport.pause();
                break;

            case AudioCommandType::transportStop:
                transport.stop();
                patternScheduler.reset();
                noteScheduler.reset();
                voicePool.stopAll();
                break;

            case AudioCommandType::setBpm:
                transport.setBpm (static_cast<double> (command.floatValue));
                break;

            case AudioCommandType::setMasterGain:
                mixer.setMasterGain (command.floatValue);
                patternScheduler.setMasterGain (command.floatValue);
                noteScheduler.setMasterGain (command.floatValue);
                break;

            case AudioCommandType::setTestToneEnabled:
                testToneEnabled.store (command.boolValue, std::memory_order_relaxed);
                break;

            case AudioCommandType::setMetronomeEnabled:
                metronomeEnabled.store (command.boolValue, std::memory_order_relaxed);
                break;

            case AudioCommandType::setLoopEnabled:
                transport.setLoopEnabled (command.boolValue);
                break;

            case AudioCommandType::setLoopStartBeats:
                transport.setLoopRangeBeats (static_cast<double> (command.floatValue),
                                             transport.getLoopEndBeats());
                break;

            case AudioCommandType::setLoopEndBeats:
                transport.setLoopRangeBeats (transport.getLoopStartBeats(),
                                             static_cast<double> (command.floatValue));
                break;

            case AudioCommandType::none:
            default:
                break;
        }
    }
}

void AudioEngine::renderTestTone (juce::AudioBuffer<float>& buffer,
                                  int startSample,
                                  int numSamples) noexcept
{
    if (! testToneEnabled.load (std::memory_order_relaxed))
        return;

    const auto phaseIncrement = (2.0 * juce::MathConstants<double>::pi * kTestToneFrequencyHz)
                                / currentSampleRate;
    auto phase = tonePhase.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto sample = static_cast<float> (std::sin (phase)) * kTestToneGain;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample (ch, startSample + i, sample);

        phase += phaseIncrement;

        if (phase > 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;
    }

    tonePhase.store (phase, std::memory_order_relaxed);
}

void AudioEngine::renderMetronome (juce::AudioBuffer<float>& buffer,
                                   int startSample,
                                   int numSamples,
                                   uint64_t blockStartPosition) noexcept
{
    if (! metronomeEnabled.load (std::memory_order_relaxed) || ! transport.isPlaying())
        return;

    const auto samplesPerBeat = transport.getSamplesPerBeat();

    if (samplesPerBeat <= 0.0)
        return;

    const auto clickSamples = static_cast<int> (currentSampleRate * kMetronomeClickMs / 1000.0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto absoluteSample = blockStartPosition + static_cast<uint64_t> (i);
        const auto beatPhase = static_cast<double> (absoluteSample % static_cast<uint64_t> (samplesPerBeat));

        if (beatPhase >= static_cast<double> (clickSamples))
            continue;

        const auto envelope = 1.0f - static_cast<float> (beatPhase / static_cast<double> (clickSamples));
        const auto click = kMetronomeGain * envelope;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample (ch, startSample + i, click);
    }
}

void AudioEngine::setOfflineRenderActive (bool active) noexcept
{
    offlineRenderActive.store (active, std::memory_order_release);
}

bool AudioEngine::isOfflineRenderActive() const noexcept
{
    return offlineRenderActive.load (std::memory_order_acquire);
}

void AudioEngine::renderOffline (juce::AudioBuffer<float>& buffer,
                                 int startSample,
                                 int numSamples) noexcept
{
    processCommands (static_cast<int> (commandScratch.size()));

    const auto blockStartPosition = transport.getSamplePosition();

    patternScheduler.process (blockStartPosition,
                              numSamples,
                              transport.isPlaying(),
                              voicePool,
                              sampleRegistry,
                              diagnostics);

    noteScheduler.process (blockStartPosition,
                           numSamples,
                           transport.isPlaying(),
                           voicePool,
                           sampleRegistry,
                           diagnostics);

    buffer.clear (startSample, numSamples);
    channelMixer.beginBlock (startSample, numSamples);

    voicePool.renderAdding (buffer, &channelMixer, startSample, numSamples, &levelMeters);
    channelMixer.mixToMaster (buffer, startSample, numSamples);
    renderMetronome (buffer, startSample, numSamples, blockStartPosition);
    mixer.processMaster (buffer, startSample, numSamples);
    transport.advance (numSamples);
}

void AudioEngine::render (juce::AudioBuffer<float>& buffer,
                          int startSample,
                          int numSamples) noexcept
{
    if (offlineRenderActive.load (std::memory_order_acquire))
    {
        buffer.clear (startSample, numSamples);
        return;
    }

    renderOffline (buffer, startSample, numSamples);

    if (voicePool.getActiveVoiceCount() == 0 && transport.isPlaying())
        renderTestTone (buffer, startSample, numSamples);

    activeVoiceCount.store (voicePool.getActiveVoiceCount(), std::memory_order_relaxed);
}

} // namespace sampr