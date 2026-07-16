#pragma once

#include <atomic>
#include <cstdint>

namespace sampr
{

class Transport
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;

    void setPlaying (bool shouldPlay) noexcept;
    bool isPlaying() const noexcept;

    void setBpm (double newBpm) noexcept;
    double getBpm() const noexcept;

    void seekToSample (uint64_t samplePosition) noexcept;
    void advance (int numSamples) noexcept;

    uint64_t getSamplePosition() const noexcept;
    double getSecondsPosition() const noexcept;

    double getSamplesPerBeat() const noexcept;
    double getSamplesPerQuarterNote() const noexcept;

    void setLoopEnabled (bool enabled) noexcept;
    bool isLoopEnabled() const noexcept;

    void setLoopRangeBeats (double startBeats, double endBeats) noexcept;
    double getLoopStartBeats() const noexcept;
    double getLoopEndBeats() const noexcept;

private:
    uint64_t wrapPosition (uint64_t position) const noexcept;

    std::atomic<bool> playing { false };
    std::atomic<double> bpm { 120.0 };
    std::atomic<uint64_t> samplePosition { 0 };
    std::atomic<bool> loopEnabled { false };
    std::atomic<double> loopStartBeats { 0.0 };
    std::atomic<double> loopEndBeats { 16.0 };
    double deviceSampleRate = 44100.0;
};

} // namespace sampr