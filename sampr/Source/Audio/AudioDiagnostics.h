#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

namespace sampr
{

enum class AudioLogCode : uint32_t
{
    none = 0,
    commandQueueFull,
    sampleNotFound,
    voicePoolExhausted,
};

// Lock-free diagnostic codes from the audio thread; drained on the UI thread.
class AudioDiagnostics
{
public:
    static constexpr int kCapacity = 64;

    void postFromAudioThread (AudioLogCode code) noexcept;
    int drainCodes (AudioLogCode* destination, int maxCodes) noexcept;

    uint32_t getCountForCode (AudioLogCode code) const noexcept;

private:
    std::array<std::atomic<uint32_t>, 8> codeCounts {};
    std::array<AudioLogCode, kCapacity> ringBuffer {};
    juce::AbstractFifo fifo { kCapacity };
};

} // namespace sampr