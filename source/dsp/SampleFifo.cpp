#include "SampleFifo.h"

#include <cstring>

namespace investigate
{

void SampleFifo::prepare (int numChannels, int newCapacityFrames)
{
    jassert (numChannels > 0);
    jassert (newCapacityFrames >= (1 << 17));

    numChannelsPrepared = numChannels;
    capacityFrames = newCapacityFrames;

    // juce::AbstractFifo always reserves one internal slot (getFreeSpace()
    // == bufferSize - getNumReady() - 1), so the underlying storage must be
    // one frame larger than the usable capacity getCapacity() advertises,
    // or a push of exactly getCapacity() frames into an empty FIFO would
    // both be wrongly dropped as an overflow AND, if the reservation were
    // ever removed upstream, index one past the end of each channel array.
    const int bufferSize = newCapacityFrames + 1;
    channels.assign (static_cast<size_t> (numChannels), std::vector<float> (static_cast<size_t> (bufferSize), 0.0f));
    fifo.setTotalSize (bufferSize);
    overflowCount.store (0, std::memory_order_relaxed);
}

void SampleFifo::reset() noexcept
{
    fifo.reset();
    overflowCount.store (0, std::memory_order_relaxed);
}

void SampleFifo::push (const float* const* channelData, int numChannels, int numFrames) noexcept
{
    if (numFrames <= 0 || numChannelsPrepared == 0)
        return;

    if (numFrames > fifo.getFreeSpace())
    {
        // Never partially write: drop the whole block and let the consumer
        // notice via the overflow counter and resynchronise.
        overflowCount.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    const int channelsToCopy = juce::jmin (numChannels, numChannelsPrepared);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (numFrames, start1, size1, start2, size2);

    for (int c = 0; c < channelsToCopy; ++c)
    {
        const float* src = channelData[c];
        float* dst = channels[static_cast<size_t> (c)].data();

        if (size1 > 0)
            std::memcpy (dst + start1, src, static_cast<size_t> (size1) * sizeof (float));
        if (size2 > 0)
            std::memcpy (dst + start2, src + size1, static_cast<size_t> (size2) * sizeof (float));
    }

    fifo.finishedWrite (size1 + size2);
}

int SampleFifo::pop (float* const* dest, int numChannels, int numFrames) noexcept
{
    if (numFrames <= 0 || numChannelsPrepared == 0)
        return 0;

    const int toRead = juce::jmin (numFrames, fifo.getNumReady());
    if (toRead <= 0)
        return 0;

    const int channelsToCopy = juce::jmin (numChannels, numChannelsPrepared);

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (toRead, start1, size1, start2, size2);

    for (int c = 0; c < channelsToCopy; ++c)
    {
        const float* src = channels[static_cast<size_t> (c)].data();
        float* dst = dest[c];

        if (size1 > 0)
            std::memcpy (dst, src + start1, static_cast<size_t> (size1) * sizeof (float));
        if (size2 > 0)
            std::memcpy (dst + size1, src + start2, static_cast<size_t> (size2) * sizeof (float));
    }

    fifo.finishedRead (size1 + size2);
    return toRead;
}

} // namespace investigate
