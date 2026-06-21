#include "Transport.h"

#include <algorithm>

namespace sampr
{

void Transport::prepare (double sampleRate) noexcept
{
    deviceSampleRate = sampleRate;
}

void Transport::reset() noexcept
{
    playing.store (false, std::memory_order_release);
    samplePosition.store (0, std::memory_order_relaxed);
}

void Transport::play() noexcept
{
    playing.store (true, std::memory_order_release);
}

void Transport::pause() noexcept
{
    playing.store (false, std::memory_order_release);
}

void Transport::stop() noexcept
{
    playing.store (false, std::memory_order_release);
    samplePosition.store (0, std::memory_order_relaxed);
}

void Transport::setPlaying (bool shouldPlay) noexcept
{
    if (shouldPlay)
        play();
    else
        pause();
}

bool Transport::isPlaying() const noexcept
{
    return playing.load (std::memory_order_acquire);
}

void Transport::setBpm (double newBpm) noexcept
{
    bpm.store (std::clamp (newBpm, 20.0, 999.0), std::memory_order_relaxed);
}

double Transport::getBpm() const noexcept
{
    return bpm.load (std::memory_order_relaxed);
}

void Transport::seekToSample (uint64_t position) noexcept
{
    samplePosition.store (wrapPosition (position), std::memory_order_relaxed);
}

uint64_t Transport::wrapPosition (uint64_t position) const noexcept
{
    if (! loopEnabled.load (std::memory_order_relaxed))
        return position;

    const auto startBeats = loopStartBeats.load (std::memory_order_relaxed);
    const auto endBeats = loopEndBeats.load (std::memory_order_relaxed);

    if (endBeats <= startBeats || deviceSampleRate <= 0.0)
        return position;

    const auto startSamples = static_cast<uint64_t> (startBeats * getSamplesPerBeat());
    const auto endSamples = static_cast<uint64_t> (endBeats * getSamplesPerBeat());
    const auto loopLength = endSamples - startSamples;

    if (loopLength == 0)
        return position;

    if (position < startSamples)
        return startSamples;

    const auto offset = (position - startSamples) % loopLength;
    return startSamples + offset;
}

void Transport::advance (int numSamples) noexcept
{
    if (! playing.load (std::memory_order_relaxed))
        return;

    const auto next = samplePosition.load (std::memory_order_relaxed)
                      + static_cast<uint64_t> (numSamples);
    samplePosition.store (wrapPosition (next), std::memory_order_relaxed);
}

uint64_t Transport::getSamplePosition() const noexcept
{
    return samplePosition.load (std::memory_order_relaxed);
}

double Transport::getSecondsPosition() const noexcept
{
    if (deviceSampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (getSamplePosition()) / deviceSampleRate;
}

double Transport::getSamplesPerBeat() const noexcept
{
    const auto currentBpm = getBpm();

    if (currentBpm <= 0.0 || deviceSampleRate <= 0.0)
        return 0.0;

    return deviceSampleRate * 60.0 / currentBpm;
}

double Transport::getSamplesPerQuarterNote() const noexcept
{
    return getSamplesPerBeat();
}

void Transport::setLoopEnabled (bool enabled) noexcept
{
    loopEnabled.store (enabled, std::memory_order_relaxed);
}

bool Transport::isLoopEnabled() const noexcept
{
    return loopEnabled.load (std::memory_order_relaxed);
}

void Transport::setLoopRangeBeats (double startBeats, double endBeats) noexcept
{
    loopStartBeats.store (std::max (0.0, startBeats), std::memory_order_relaxed);
    loopEndBeats.store (std::max (startBeats + 0.25, endBeats), std::memory_order_relaxed);
}

double Transport::getLoopStartBeats() const noexcept
{
    return loopStartBeats.load (std::memory_order_relaxed);
}

double Transport::getLoopEndBeats() const noexcept
{
    return loopEndBeats.load (std::memory_order_relaxed);
}

} // namespace sampr