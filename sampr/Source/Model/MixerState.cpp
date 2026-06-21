#include "MixerState.h"

namespace sampr
{

MixerState::MixerState()
{
    state.setProperty (masterGainId, 1.0f, nullptr);
    state.setProperty (metronomeId, false, nullptr);
    state.setProperty (loopEnabledId, false, nullptr);
    state.setProperty (loopStartId, 0.0, nullptr);
    state.setProperty (loopEndId, 16.0, nullptr);
}

void MixerState::setChangeCallback (ChangeCallback callback)
{
    changeCallback = std::move (callback);
}

void MixerState::notifyChanged()
{
    if (changeCallback != nullptr)
        changeCallback();
}

void MixerState::syncFromProjectSettings (double bpm,
                                          float masterGain,
                                          bool metronome,
                                          bool loopEnabled,
                                          double loopStartBeats,
                                          double loopEndBeats)
{
    juce::ignoreUnused (bpm);
    state.setProperty (masterGainId, masterGain, nullptr);
    state.setProperty (metronomeId, metronome, nullptr);
    state.setProperty (loopEnabledId, loopEnabled, nullptr);
    state.setProperty (loopStartId, loopStartBeats, nullptr);
    state.setProperty (loopEndId, loopEndBeats, nullptr);
    notifyChanged();
}

} // namespace sampr