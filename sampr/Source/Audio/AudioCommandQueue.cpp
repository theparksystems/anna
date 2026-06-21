#include "AudioCommandQueue.h"

namespace sampr
{

bool AudioCommandQueue::push (const AudioCommand& command) noexcept
{
    const auto scope = fifo.write (1);

    if (scope.blockSize1 <= 0)
    {
        droppedCount.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    storage[static_cast<size_t> (scope.startIndex1)] = command;
    return true;
}

int AudioCommandQueue::drain (AudioCommand* destination, int maxCommands) noexcept
{
    const auto available = fifo.getNumReady();
    const auto toRead = juce::jmin (available, maxCommands);

    if (toRead <= 0 || destination == nullptr)
        return 0;

    const auto scope = fifo.read (toRead);
    auto outIndex = 0;

    if (scope.blockSize1 > 0)
    {
        for (int i = 0; i < scope.blockSize1; ++i)
            destination[outIndex++] = storage[static_cast<size_t> (scope.startIndex1 + i)];
    }

    if (scope.blockSize2 > 0)
    {
        for (int i = 0; i < scope.blockSize2; ++i)
            destination[outIndex++] = storage[static_cast<size_t> (scope.startIndex2 + i)];
    }

    return outIndex;
}

} // namespace sampr