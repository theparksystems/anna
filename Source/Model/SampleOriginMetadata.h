#pragma once

#include <juce_core/juce_core.h>

namespace sampr
{

struct SampleOriginMetadata
{
    juce::String sourceType;
    juce::String sourceUrl;
    juce::String sourceTitle;
    juce::String sourceAuthor;
    juce::String sourceId;
    juce::String downloadedAt;
    juce::String localFileName;

    bool hasSource() const noexcept
    {
        return sourceUrl.isNotEmpty() || sourceTitle.isNotEmpty();
    }
};

} // namespace sampr
