#pragma once

#include <juce_core/juce_core.h>

#include "SampleAsset.h"

namespace sampr
{

using NoteId = uint32_t;
inline constexpr NoteId kInvalidNoteId = 0;

inline constexpr int kPianoRollRootPitch = 60;
inline constexpr int kPianoRollLowPitch = 36;
inline constexpr int kPianoRollHighPitch = 84;

struct NoteEvent
{
    NoteId id = kInvalidNoteId;
    double startBeats = 0.0;
    double durationBeats = 0.25;
    int pitch = kPianoRollRootPitch;
    float velocity = 0.85f;
    juce::String sampleRefId;
    AssetId assetId = kInvalidAssetId;
    int sliceIndex = 0;
};

} // namespace sampr