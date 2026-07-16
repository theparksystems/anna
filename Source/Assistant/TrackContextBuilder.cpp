#include "TrackContextBuilder.h"

#include <cmath>

#include "../Audio/LevelMeterBank.h"
#include "../Audio/PatternSnapshot.h"
#include "../Model/Pattern.h"
#include "../Model/ProjectModel.h"
#include "../Model/SampleAsset.h"

namespace sampr
{

namespace
{
    juce::var makeObject()
    {
        return juce::var (new juce::DynamicObject());
    }

    void setNum (juce::DynamicObject& obj, const juce::Identifier& key, double value)
    {
        obj.setProperty (key, value);
    }

    void setBool (juce::DynamicObject& obj, const juce::Identifier& key, bool value)
    {
        obj.setProperty (key, value);
    }

    void setStr (juce::DynamicObject& obj, const juce::Identifier& key, const juce::String& value)
    {
        obj.setProperty (key, value);
    }

    juce::var eqToVar (const ChannelEqState& eq)
    {
        auto* obj = new juce::DynamicObject();
        setBool (*obj, "enabled", eq.enabled);
        setNum (*obj, "lowFreqHz", eq.low.frequencyHz);
        setNum (*obj, "lowGainDb", eq.low.gainDb);
        setNum (*obj, "lowQ", eq.low.q);
        setNum (*obj, "midFreqHz", eq.mid.frequencyHz);
        setNum (*obj, "midGainDb", eq.mid.gainDb);
        setNum (*obj, "midQ", eq.mid.q);
        setNum (*obj, "highFreqHz", eq.high.frequencyHz);
        setNum (*obj, "highGainDb", eq.high.gainDb);
        setNum (*obj, "highQ", eq.high.q);
        return juce::var (obj);
    }

    juce::var compressorToVar (const ChannelCompressorState& comp)
    {
        auto* obj = new juce::DynamicObject();
        setBool (*obj, "enabled", comp.enabled);
        setNum (*obj, "thresholdDb", comp.thresholdDb);
        setNum (*obj, "ratio", comp.ratio);
        setNum (*obj, "attackMs", comp.attackMs);
        setNum (*obj, "releaseMs", comp.releaseMs);
        setNum (*obj, "makeupGainDb", comp.makeupGainDb);
        return juce::var (obj);
    }

    juce::var colorToVar (const ChannelColorState& color)
    {
        auto* obj = new juce::DynamicObject();
        setBool (*obj, "enabled", color.enabled);
        setNum (*obj, "lowCutHz", color.lowCutHz);
        setNum (*obj, "highCutHz", color.highCutHz);
        setNum (*obj, "drive", color.drive);
        setNum (*obj, "width", color.width);
        setNum (*obj, "chorusMix", color.chorusMix);
        setNum (*obj, "chorusRateHz", color.chorusRateHz);
        setNum (*obj, "phaserMix", color.phaserMix);
        setNum (*obj, "phaserRateHz", color.phaserRateHz);
        setNum (*obj, "pump", color.pump);
        setNum (*obj, "limiterCeilingDb", color.limiterCeilingDb);
        return juce::var (obj);
    }

    juce::var delayToVar (const ChannelDelayState& delay)
    {
        auto* obj = new juce::DynamicObject();
        setBool (*obj, "enabled", delay.enabled);
        setNum (*obj, "timeMs", delay.timeMs);
        setNum (*obj, "feedback", delay.feedback);
        setNum (*obj, "mix", delay.mix);
        return juce::var (obj);
    }

    juce::var reverbToVar (const ChannelReverbState& reverb)
    {
        auto* obj = new juce::DynamicObject();
        setBool (*obj, "enabled", reverb.enabled);
        setNum (*obj, "roomSize", reverb.roomSize);
        setNum (*obj, "damping", reverb.damping);
        setNum (*obj, "wetLevel", reverb.wetLevel);
        setNum (*obj, "dryLevel", reverb.dryLevel);
        setNum (*obj, "width", reverb.width);
        return juce::var (obj);
    }
}

juce::String TrackContextBuilder::scopeToString (ContextScope scope)
{
    switch (scope)
    {
        case ContextScope::project: return "project";
        case ContextScope::pattern: return "pattern";
        case ContextScope::channel: return "channel";
        case ContextScope::slice:   return "slice";
    }

    return "channel";
}

juce::var TrackContextBuilder::buildStepStats (const std::vector<Step>& steps)
{
    auto* obj = new juce::DynamicObject();
    int activeCount = 0;
    float sum = 0.0f;
    float minVel = 1.0f;
    float maxVel = 0.0f;

    for (const auto& step : steps)
    {
        if (! step.active)
            continue;

        ++activeCount;
        sum += step.velocity;
        minVel = std::min (minVel, step.velocity);
        maxVel = std::max (maxVel, step.velocity);
    }

    setNum (*obj, "activeSteps", activeCount);
    setNum (*obj, "totalSteps", static_cast<int> (steps.size()));
    setNum (*obj, "density", steps.empty() ? 0.0 : static_cast<double> (activeCount) / static_cast<double> (steps.size()));

    if (activeCount > 0)
    {
        const auto avg = sum / static_cast<float> (activeCount);
        setNum (*obj, "avgVelocity", avg);
        setNum (*obj, "minVelocity", minVel);
        setNum (*obj, "maxVelocity", maxVel);

        float variance = 0.0f;
        for (const auto& step : steps)
        {
            if (! step.active)
                continue;

            const auto diff = step.velocity - avg;
            variance += diff * diff;
        }

        variance /= static_cast<float> (activeCount);
        setNum (*obj, "velocityVariance", variance);
        setNum (*obj, "dynamicRangeScore", static_cast<double> (maxVel - minVel));
    }

    return juce::var (obj);
}

juce::var TrackContextBuilder::buildDerivedStats (const PatternRow& row, float peakLevel)
{
    auto* obj = new juce::DynamicObject();

    int fxActive = 0;
    if (row.channelColor.enabled) ++fxActive;
    if (row.channelEq.enabled) ++fxActive;
    if (row.channelCompressor.enabled) ++fxActive;
    if (row.channelDelay.enabled) ++fxActive;
    if (row.channelReverb.enabled) ++fxActive;

    const auto eqBalance = row.channelEq.low.gainDb + row.channelEq.mid.gainDb + row.channelEq.high.gainDb;
    setNum (*obj, "fxActiveCount", fxActive);
    setNum (*obj, "eqBoostCutBalanceDb", eqBalance);
    setBool (*obj, "isLikelyClipping", peakLevel > 0.95f);
    setBool (*obj, "isMuted", row.channelMute);
    setBool (*obj, "isSolo", row.channelSolo);
    setNum (*obj, "stereoPan", row.channelPan);

    return juce::var (obj);
}

juce::var TrackContextBuilder::buildChannelSection (const PatternRow& row,
                                                    int channelIndex,
                                                    const LevelMeterBank* meters)
{
    auto* obj = new juce::DynamicObject();
    setNum (*obj, "index", channelIndex);
    setStr (*obj, "label", row.label.isNotEmpty() ? row.label : ("Channel " + juce::String (channelIndex + 1)));
    setNum (*obj, "gain", row.channelGain);
    setNum (*obj, "pan", row.channelPan);
    setBool (*obj, "mute", row.channelMute);
    setBool (*obj, "solo", row.channelSolo);
    setNum (*obj, "sliceIndex", row.sliceIndex);

    float peak = 0.0f;
    if (meters != nullptr && juce::isPositiveAndBelow (channelIndex, kMaxPatternChannels))
        peak = meters->getPeak (channelIndex);

    setNum (*obj, "peakLevel", peak);
    obj->setProperty ("color", colorToVar (row.channelColor));
    obj->setProperty ("eq", eqToVar (row.channelEq));
    obj->setProperty ("compressor", compressorToVar (row.channelCompressor));
    obj->setProperty ("delay", delayToVar (row.channelDelay));
    obj->setProperty ("reverb", reverbToVar (row.channelReverb));
    obj->setProperty ("steps", buildStepStats (row.steps));
    obj->setProperty ("derived", buildDerivedStats (row, peak));

    return juce::var (obj);
}

juce::var TrackContextBuilder::buildSliceSection (const SampleAsset& asset, int sliceIndex)
{
    auto* obj = new juce::DynamicObject();
    setStr (*obj, "sampleName", asset.displayName);
    setNum (*obj, "sampleRate", asset.sampleRate);
    setNum (*obj, "numChannels", asset.numChannels);
    setNum (*obj, "numSamples", asset.numSamples);
    setNum (*obj, "durationSec", asset.sampleRate > 0.0 ? asset.numSamples / asset.sampleRate : 0.0);
    setNum (*obj, "sliceCount", static_cast<int> (asset.slices.size()));
    setNum (*obj, "selectedSliceIndex", sliceIndex);

    if (juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset.slices.size())))
    {
        const auto& slice = asset.slices[static_cast<size_t> (sliceIndex)];
        auto* sliceObj = new juce::DynamicObject();
        setNum (*sliceObj, "startSample", slice.startSample);
        setNum (*sliceObj, "endSample", slice.endSample);
        setNum (*sliceObj, "lengthSamples", slice.endSample - slice.startSample);
        setNum (*sliceObj, "pitchSemitones", slice.pitchSemitones);
        setNum (*sliceObj, "timeRatio", slice.timeRatio);
        setBool (*sliceObj, "loop", slice.loop);
        setBool (*sliceObj, "bakeReady", slice.bakeReady);
        obj->setProperty ("slice", juce::var (sliceObj));
    }

    return juce::var (obj);
}

juce::var TrackContextBuilder::buildPatternSection (const Pattern& pattern, int patternIndex)
{
    auto* obj = new juce::DynamicObject();
    setNum (*obj, "index", patternIndex);
    setStr (*obj, "name", pattern.name);
    setNum (*obj, "numSteps", pattern.numSteps);
    setNum (*obj, "lengthBars", pattern.lengthBars);
    setNum (*obj, "channelCount", static_cast<int> (pattern.rows.size()));
    setNum (*obj, "noteCount", static_cast<int> (pattern.notes.size()));
    return juce::var (obj);
}

juce::var TrackContextBuilder::buildProjectSection (const TrackContextInput& input)
{
    auto* obj = new juce::DynamicObject();

    if (input.project != nullptr)
    {
        const auto& settings = input.project->getSettings();
        setStr (*obj, "projectName", settings.projectName);
        setNum (*obj, "bpm", settings.bpm);
        setNum (*obj, "masterGain", settings.masterGain);
        setStr (*obj, "playbackMode", settings.playbackMode == PlaybackMode::song ? "song" : "pattern");
        setBool (*obj, "metronomeEnabled", settings.metronomeEnabled);
        setBool (*obj, "loopEnabled", settings.loopEnabled);
    }

    setNum (*obj, "bpm", input.bpm);
    setBool (*obj, "transportPlaying", input.transportPlaying);

    if (input.meters != nullptr)
        setNum (*obj, "masterPeak", input.meters->getMasterPeak());

    return juce::var (obj);
}

juce::var TrackContextBuilder::build (const TrackContextInput& input)
{
    auto* root = new juce::DynamicObject();
    setStr (*root, "scope", scopeToString (input.scope));
    root->setProperty ("project", buildProjectSection (input));

    if (input.pattern != nullptr)
    {
        root->setProperty ("pattern", buildPatternSection (*input.pattern, input.patternIndex));

        if (input.scope == ContextScope::pattern)
        {
            juce::Array<juce::var> channels;

            for (int i = 0; i < static_cast<int> (input.pattern->rows.size()); ++i)
                channels.add (buildChannelSection (input.pattern->rows[static_cast<size_t> (i)], i, input.meters));

            root->setProperty ("channels", channels);
        }
        else if (input.scope == ContextScope::channel
                 && juce::isPositiveAndBelow (input.channelIndex, static_cast<int> (input.pattern->rows.size())))
        {
            root->setProperty ("channel",
                               buildChannelSection (input.pattern->rows[static_cast<size_t> (input.channelIndex)],
                                                    input.channelIndex,
                                                    input.meters));
        }
    }

    if (input.scope == ContextScope::slice && input.sampleAsset != nullptr)
        root->setProperty ("slice", buildSliceSection (*input.sampleAsset, input.sliceIndex));

    return juce::var (root);
}

} // namespace sampr
