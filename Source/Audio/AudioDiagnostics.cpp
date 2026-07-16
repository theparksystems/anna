#include "AudioDiagnostics.h"

namespace sampr
{

namespace
{
    constexpr int codeIndex (AudioLogCode code) noexcept
    {
        return static_cast<int> (code);
    }
}

void AudioDiagnostics::postFromAudioThread (AudioLogCode code) noexcept
{
    if (code == AudioLogCode::none)
        return;

    const auto index = codeIndex (code);

    if (index >= 0 && index < static_cast<int> (codeCounts.size()))
        codeCounts[static_cast<size_t> (index)].fetch_add (1, std::memory_order_relaxed);

    const auto scope = fifo.write (1);

    if (scope.blockSize1 > 0)
        ringBuffer[static_cast<size_t> (scope.startIndex1)] = code;
}

int AudioDiagnostics::drainCodes (AudioLogCode* destination, int maxCodes) noexcept
{
    const auto toRead = juce::jmin (fifo.getNumReady(), maxCodes);

    if (toRead <= 0 || destination == nullptr)
        return 0;

    const auto scope = fifo.read (toRead);
    auto outIndex = 0;

    if (scope.blockSize1 > 0)
    {
        for (int i = 0; i < scope.blockSize1; ++i)
            destination[outIndex++] = ringBuffer[static_cast<size_t> (scope.startIndex1 + i)];
    }

    if (scope.blockSize2 > 0)
    {
        for (int i = 0; i < scope.blockSize2; ++i)
            destination[outIndex++] = ringBuffer[static_cast<size_t> (scope.startIndex2 + i)];
    }

    return outIndex;
}

uint32_t AudioDiagnostics::getCountForCode (AudioLogCode code) const noexcept
{
    const auto index = codeIndex (code);

    if (index < 0 || index >= static_cast<int> (codeCounts.size()))
        return 0;

    return codeCounts[static_cast<size_t> (index)].load (std::memory_order_relaxed);
}

} // namespace sampr