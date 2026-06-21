#pragma once

#include <functional>

#include <juce_data_structures/juce_data_structures.h>

namespace sampr
{

// ValueTree-backed transport/mixer settings for reactive UI binding.
class MixerState
{
public:
    using ChangeCallback = std::function<void()>;

    static inline const juce::Identifier masterGainId { "masterGain" };
    static inline const juce::Identifier metronomeId { "metronomeEnabled" };
    static inline const juce::Identifier loopEnabledId { "loopEnabled" };
    static inline const juce::Identifier loopStartId { "loopStartBeats" };
    static inline const juce::Identifier loopEndId { "loopEndBeats" };

    MixerState();

    void setChangeCallback (ChangeCallback callback);
    juce::ValueTree& getStateTree() noexcept { return state; }
    const juce::ValueTree& getStateTree() const noexcept { return state; }

    void syncFromProjectSettings (double bpm,
                                  float masterGain,
                                  bool metronome,
                                  bool loopEnabled,
                                  double loopStartBeats,
                                  double loopEndBeats);

private:
    void notifyChanged();

    juce::ValueTree state { "mixerState" };
    ChangeCallback changeCallback;
};

} // namespace sampr