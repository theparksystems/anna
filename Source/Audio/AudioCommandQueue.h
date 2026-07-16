#pragma once

#include <array>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "AudioCommand.h"

namespace sampr
{

// Single-producer (UI) / single-consumer (audio) command queue.
class AudioCommandQueue
{
public:
    static constexpr int kCapacity = 256;

    bool push (const AudioCommand& command) noexcept;
    int drain (AudioCommand* destination, int maxCommands) noexcept;

    uint32_t getDroppedCount() const noexcept
    {
        return droppedCount.load (std::memory_order_relaxed);
    }

    void resetDroppedCount() noexcept
    {
        droppedCount.store (0, std::memory_order_relaxed);
    }

private:
    std::array<AudioCommand, kCapacity> storage {};
    juce::AbstractFifo fifo { kCapacity };
    std::atomic<uint32_t> droppedCount { 0 };
};

} // namespace sampr