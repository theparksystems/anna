#include "PatternStore.h"

#include <algorithm>

namespace sampr
{

namespace
{
    juce::Colour colourForRow (int index)
    {
        static constexpr juce::uint32 colours[] {
            0xff4cc2ff, 0xffff6b6b, 0xffa3e635, 0xffffb347,
            0xffc084fc, 0xff22d3ee, 0xfff472b6, 0xff94a3b8
        };
        return juce::Colour (colours[static_cast<size_t> (index) % 8]);
    }
}

PatternStore::PatternStore (ProjectModel& projectModel, SampleManager& samples)
    : project (projectModel),
      sampleManager (samples)
{
}

void PatternStore::setChangeCallback (ChangeCallback callback)
{
    changeCallback = std::move (callback);
}

void PatternStore::setUndoCheckpoint (std::function<void()> callback)
{
    undoCheckpoint = std::move (callback);
}

void PatternStore::saveCheckpoint()
{
    if (undoCheckpoint != nullptr)
        undoCheckpoint();
}

void PatternStore::onProjectStructureChanged()
{
    notifyChanged();
}

void PatternStore::notifyChanged()
{
    if (changeCallback != nullptr)
        changeCallback();
}

int PatternStore::getCurrentPatternIndex() const noexcept
{
    return project.getSettings().currentPatternIndex;
}

Pattern& PatternStore::getCurrentPattern()
{
    return project.getPatterns()[static_cast<size_t> (getCurrentPatternIndex())];
}

const Pattern& PatternStore::getCurrentPattern() const
{
    return project.getPatterns()[static_cast<size_t> (getCurrentPatternIndex())];
}

const Pattern& PatternStore::getPattern (int index) const
{
    return project.getPatterns()[static_cast<size_t> (index)];
}

void PatternStore::selectPattern (int index)
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (project.getPatterns().size())))
        return;

    project.getSettings().currentPatternIndex = index;
    notifyChanged();
}

void PatternStore::addPattern()
{
    auto& patterns = project.getPatterns();

    if (static_cast<int> (patterns.size()) >= ProjectModel::kMaxPatterns)
        return;

    saveCheckpoint();
    Pattern pattern;
    pattern.id = project.allocatePatternId();
    pattern.name = "Pattern " + juce::String (patterns.size() + 1);
    pattern.numSteps = getCurrentPattern().numSteps;
    patterns.push_back (std::move (pattern));
    project.getSettings().currentPatternIndex = static_cast<int> (patterns.size()) - 1;
    notifyChanged();
}

void PatternStore::duplicateCurrentPattern()
{
    auto& patterns = project.getPatterns();

    if (static_cast<int> (patterns.size()) >= ProjectModel::kMaxPatterns)
        return;

    saveCheckpoint();
    auto copy = getCurrentPattern();
    copy.id = project.allocatePatternId();
    copy.name = getCurrentPattern().name + " Copy";
    patterns.push_back (std::move (copy));
    project.getSettings().currentPatternIndex = static_cast<int> (patterns.size()) - 1;
    notifyChanged();
}

void PatternStore::setPatternName (int index, const juce::String& name)
{
    auto& patterns = project.getPatterns();

    if (! juce::isPositiveAndBelow (index, static_cast<int> (patterns.size())))
        return;

    saveCheckpoint();
    patterns[static_cast<size_t> (index)].name = name;
    notifyChanged();
}

void PatternStore::ensureStepArrays (Pattern& pattern)
{
    for (auto& row : pattern.rows)
    {
        if (static_cast<int> (row.steps.size()) != pattern.numSteps)
            row.steps.assign (static_cast<size_t> (pattern.numSteps), Step {});
    }
}

void PatternStore::setNumSteps (int numSteps)
{
    numSteps = juce::jlimit (16, kMaxPatternSteps, numSteps);
    saveCheckpoint();
    auto& pattern = getCurrentPattern();
    pattern.numSteps = numSteps;
    ensureStepArrays (pattern);
    notifyChanged();
}

void PatternStore::activateStep (int rowIndex, int stepIndex, float velocity)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    ensureStepArrays (pattern);

    if (! juce::isPositiveAndBelow (stepIndex, pattern.numSteps))
        return;

    auto& step = pattern.rows[static_cast<size_t> (rowIndex)].steps[static_cast<size_t> (stepIndex)];
    step.active = true;
    step.velocity = juce::jlimit (0.05f, 1.0f, velocity);
    notifyChanged();
}

void PatternStore::toggleStep (int rowIndex, int stepIndex)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    ensureStepArrays (pattern);

    if (! juce::isPositiveAndBelow (stepIndex, pattern.numSteps))
        return;

    auto& step = pattern.rows[static_cast<size_t> (rowIndex)].steps[static_cast<size_t> (stepIndex)];
    step.active = ! step.active;
    notifyChanged();
}

void PatternStore::setStepVelocity (int rowIndex, int stepIndex, float velocity)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    ensureStepArrays (pattern);

    if (! juce::isPositiveAndBelow (stepIndex, pattern.numSteps))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].steps[static_cast<size_t> (stepIndex)].velocity
        = juce::jlimit (0.05f, 1.0f, velocity);
    notifyChanged();
}

void PatternStore::setStepProbability (int rowIndex, int stepIndex, float probability)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    ensureStepArrays (pattern);

    if (! juce::isPositiveAndBelow (stepIndex, pattern.numSteps))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].steps[static_cast<size_t> (stepIndex)].probability
        = juce::jlimit (0.0f, 1.0f, probability);
    notifyChanged();
}

juce::String PatternStore::refIdForAsset (AssetId assetId) const
{
    if (const auto* asset = sampleManager.getAsset (assetId))
        return asset->refId.isNotEmpty() ? asset->refId : ("asset-" + juce::String (assetId));

    return {};
}

void PatternStore::setChannelGain (int rowIndex, float gain)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelGain
        = juce::jlimit (0.0f, 2.0f, gain);
    notifyChanged();
}

void PatternStore::setChannelPan (int rowIndex, float pan)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelPan
        = juce::jlimit (-1.0f, 1.0f, pan);
    notifyChanged();
}

void PatternStore::setChannelMute (int rowIndex, bool muted)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelMute = muted;
    notifyChanged();
}

void PatternStore::setChannelSolo (int rowIndex, bool solo)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelSolo = solo;
    notifyChanged();
}

void PatternStore::toggleChannelMute (int rowIndex)
{
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    setChannelMute (rowIndex, ! pattern.rows[static_cast<size_t> (rowIndex)].channelMute);
}

void PatternStore::toggleChannelSolo (int rowIndex)
{
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    setChannelSolo (rowIndex, ! pattern.rows[static_cast<size_t> (rowIndex)].channelSolo);
}

ChannelEqState PatternStore::getChannelEq (int rowIndex) const
{
    const auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return makeDefaultChannelEq();

    return pattern.rows[static_cast<size_t> (rowIndex)].channelEq;
}

void PatternStore::setChannelEqEnabled (int rowIndex, bool enabled)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelEq.enabled = enabled;
    notifyChanged();
}

void PatternStore::setChannelEqBand (int rowIndex,
                                   EqBandKind band,
                                   float frequencyHz,
                                   float gainDb,
                                   float q)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    auto& eq = pattern.rows[static_cast<size_t> (rowIndex)].channelEq;
    EqBandState* target = nullptr;

    switch (band)
    {
        case EqBandKind::lowShelf:  target = &eq.low;  break;
        case EqBandKind::peak:      target = &eq.mid;  break;
        case EqBandKind::highShelf: target = &eq.high; break;
    }

    if (target == nullptr)
        return;

    target->frequencyHz = juce::jlimit (20.0f, 20000.0f, frequencyHz);
    target->gainDb = juce::jlimit (-24.0f, 24.0f, gainDb);
    target->q = juce::jlimit (0.1f, 10.0f, q);
    notifyChanged();
}

ChannelCompressorState PatternStore::getChannelCompressor (int rowIndex) const
{
    const auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return {};

    return pattern.rows[static_cast<size_t> (rowIndex)].channelCompressor;
}

void PatternStore::setChannelCompressorEnabled (int rowIndex, bool enabled)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelCompressor.enabled = enabled;
    notifyChanged();
}

void PatternStore::setChannelCompressor (int rowIndex, const ChannelCompressorState& state)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    auto& comp = pattern.rows[static_cast<size_t> (rowIndex)].channelCompressor;
    comp.enabled = state.enabled;
    comp.thresholdDb = juce::jlimit (-60.0f, 0.0f, state.thresholdDb);
    comp.ratio = juce::jlimit (1.0f, 20.0f, state.ratio);
    comp.attackMs = juce::jlimit (0.1f, 200.0f, state.attackMs);
    comp.releaseMs = juce::jlimit (1.0f, 1000.0f, state.releaseMs);
    comp.makeupGainDb = juce::jlimit (0.0f, 24.0f, state.makeupGainDb);
    notifyChanged();
}

ChannelDelayState PatternStore::getChannelDelay (int rowIndex) const
{
    const auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return {};

    return pattern.rows[static_cast<size_t> (rowIndex)].channelDelay;
}

ChannelColorState PatternStore::getChannelColor (int rowIndex) const
{
    const auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return {};

    return pattern.rows[static_cast<size_t> (rowIndex)].channelColor;
}

void PatternStore::setChannelColorEnabled (int rowIndex, bool enabled)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelColor.enabled = enabled;
    notifyChanged();
}

void PatternStore::setChannelColor (int rowIndex, const ChannelColorState& state)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    auto& color = pattern.rows[static_cast<size_t> (rowIndex)].channelColor;
    color.enabled = state.enabled;
    color.lowCutHz = juce::jlimit (20.0f, 1000.0f, state.lowCutHz);
    color.highCutHz = juce::jlimit (1000.0f, 20000.0f, state.highCutHz);
    color.drive = juce::jlimit (0.0f, 1.0f, state.drive);
    color.width = juce::jlimit (0.0f, 2.0f, state.width);
    color.chorusMix = juce::jlimit (0.0f, 1.0f, state.chorusMix);
    color.chorusRateHz = juce::jlimit (0.05f, 5.0f, state.chorusRateHz);
    color.phaserMix = juce::jlimit (0.0f, 1.0f, state.phaserMix);
    color.phaserRateHz = juce::jlimit (0.05f, 5.0f, state.phaserRateHz);
    color.pump = juce::jlimit (0.0f, 1.0f, state.pump);
    color.limiterCeilingDb = juce::jlimit (-12.0f, 0.0f, state.limiterCeilingDb);
    notifyChanged();
}

void PatternStore::setChannelDelayEnabled (int rowIndex, bool enabled)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelDelay.enabled = enabled;
    notifyChanged();
}

void PatternStore::setChannelDelay (int rowIndex, const ChannelDelayState& state)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    auto& delay = pattern.rows[static_cast<size_t> (rowIndex)].channelDelay;
    delay.enabled = state.enabled;
    delay.timeMs = juce::jlimit (1.0f, 2000.0f, state.timeMs);
    delay.feedback = juce::jlimit (0.0f, 0.95f, state.feedback);
    delay.mix = juce::jlimit (0.0f, 1.0f, state.mix);
    notifyChanged();
}

ChannelReverbState PatternStore::getChannelReverb (int rowIndex) const
{
    const auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return {};

    return pattern.rows[static_cast<size_t> (rowIndex)].channelReverb;
}

void PatternStore::setChannelReverbEnabled (int rowIndex, bool enabled)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows[static_cast<size_t> (rowIndex)].channelReverb.enabled = enabled;
    notifyChanged();
}

void PatternStore::setChannelReverb (int rowIndex, const ChannelReverbState& state)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    auto& reverb = pattern.rows[static_cast<size_t> (rowIndex)].channelReverb;
    reverb.enabled = state.enabled;
    reverb.roomSize = juce::jlimit (0.0f, 1.0f, state.roomSize);
    reverb.damping = juce::jlimit (0.0f, 1.0f, state.damping);
    reverb.wetLevel = juce::jlimit (0.0f, 1.0f, state.wetLevel);
    reverb.dryLevel = juce::jlimit (0.0f, 1.0f, state.dryLevel);
    reverb.width = juce::jlimit (0.0f, 1.0f, state.width);
    notifyChanged();
}

Pattern PatternStore::exportCurrentPattern() const
{
    return getCurrentPattern();
}

bool PatternStore::importPatternIntoCurrent (const Pattern& imported, bool replaceRows)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();
    pattern.name = imported.name;
    pattern.numSteps = imported.numSteps;
    pattern.lengthBars = imported.lengthBars;
    pattern.notes = imported.notes;

    if (replaceRows)
        pattern.rows = imported.rows;
    else
    {
        const auto rowCount = juce::jmin (static_cast<int> (pattern.rows.size()),
                                          static_cast<int> (imported.rows.size()));

        for (int i = 0; i < rowCount; ++i)
        {
            auto& row = pattern.rows[static_cast<size_t> (i)];
            const auto& src = imported.rows[static_cast<size_t> (i)];
            row.steps = src.steps;
            row.channelEq = src.channelEq;
            row.channelColor = src.channelColor;
            row.channelCompressor = src.channelCompressor;
            row.channelDelay = src.channelDelay;
            row.channelReverb = src.channelReverb;
            row.channelGain = src.channelGain;
            row.channelPan = src.channelPan;
            row.channelMute = src.channelMute;
            row.channelSolo = src.channelSolo;
        }
    }

    ensureStepArrays (pattern);
    notifyChanged();
    return true;
}

int PatternStore::addRowFromAsset (AssetId assetId, int sliceIndex)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (static_cast<int> (pattern.rows.size()) >= kMaxPatternChannels)
        return -1;

    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return -1;

    PatternRow row;
    row.sampleRefId = refIdForAsset (assetId);
    row.assetId = assetId;
    row.sliceIndex = juce::jlimit (0, juce::jmax (0, static_cast<int> (asset->slices.size()) - 1), sliceIndex);
    row.label = asset->displayName;
    row.colour = colourForRow (static_cast<int> (pattern.rows.size()));
    row.steps.assign (static_cast<size_t> (pattern.numSteps), Step {});
    pattern.rows.push_back (std::move (row));
    notifyChanged();
    return static_cast<int> (pattern.rows.size()) - 1;
}

int PatternStore::addRowFromSelectedAsset()
{
    return addRowFromAsset (sampleManager.getSelectedAssetId(), 0);
}

void PatternStore::removeRow (int rowIndex)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    pattern.rows.erase (pattern.rows.begin() + rowIndex);
    notifyChanged();
}

void PatternStore::clearRow (int rowIndex)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    ensureStepArrays (pattern);

    for (auto& step : pattern.rows[static_cast<size_t> (rowIndex)].steps)
        step = Step {};

    notifyChanged();
}

void PatternStore::clearPattern()
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    for (auto& row : pattern.rows)
    {
        ensureStepArrays (pattern);
        for (auto& step : row.steps)
            step = Step {};
    }

    pattern.notes.clear();
    notifyChanged();
}

void PatternStore::clearNotes()
{
    saveCheckpoint();
    getCurrentPattern().notes.clear();
    notifyChanged();
}

void PatternStore::setLengthBars (int bars)
{
    saveCheckpoint();
    getCurrentPattern().lengthBars = juce::jlimit (1, 8, bars);
    notifyChanged();
}

NoteId PatternStore::addNote (const NoteEvent& prototype)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();

    if (static_cast<int> (pattern.notes.size()) >= kMaxScheduledNotes)
        return kInvalidNoteId;

    NoteEvent note = prototype;
    note.id = project.allocateNoteId();

    if (note.sampleRefId.isEmpty() && note.assetId != kInvalidAssetId)
        note.sampleRefId = refIdForAsset (note.assetId);

    pattern.notes.push_back (note);
    notifyChanged();
    return note.id;
}

void PatternStore::updateNote (const NoteEvent& note)
{
    if (auto* existing = findNote (note.id))
    {
        saveCheckpoint();
        *existing = note;

        if (existing->sampleRefId.isEmpty() && existing->assetId != kInvalidAssetId)
            existing->sampleRefId = refIdForAsset (existing->assetId);

        notifyChanged();
    }
}

void PatternStore::deleteNote (NoteId noteId)
{
    saveCheckpoint();
    auto& pattern = getCurrentPattern();
    pattern.notes.erase (std::remove_if (pattern.notes.begin(), pattern.notes.end(),
                                         [noteId] (const NoteEvent& n) { return n.id == noteId; }),
                         pattern.notes.end());
    notifyChanged();
}

NoteEvent* PatternStore::findNote (NoteId noteId)
{
    auto& pattern = getCurrentPattern();

    for (auto& note : pattern.notes)
        if (note.id == noteId)
            return &note;

    return nullptr;
}

const NoteEvent* PatternStore::findNote (NoteId noteId) const
{
    const auto& pattern = getCurrentPattern();

    for (const auto& note : pattern.notes)
        if (note.id == noteId)
            return &note;

    return nullptr;
}

void PatternStore::syncRowsFromLoadedAssets()
{
    const auto& ids = sampleManager.getAssetIds();

    for (const auto assetId : ids)
    {
        bool exists = false;

        for (const auto& row : getCurrentPattern().rows)
        {
            if (row.assetId == assetId)
            {
                exists = true;
                break;
            }
        }

        if (! exists)
            addRowFromAsset (assetId, 0);
    }

    notifyChanged();
}

SharedPatternSnapshot PatternStore::buildSnapshotForPattern (int patternIndex,
                                                               double bpm,
                                                               double sampleRate) const
{
    const auto& patterns = project.getPatterns();

    if (! juce::isPositiveAndBelow (patternIndex, static_cast<int> (patterns.size())))
        return std::make_shared<PatternSnapshot>();

    const auto& pattern = patterns[static_cast<size_t> (patternIndex)];
    auto snap = std::make_shared<PatternSnapshot>();
    snap->numSteps = pattern.numSteps;
    snap->numChannels = juce::jmin (static_cast<int> (pattern.rows.size()), kMaxPatternChannels);
    snap->anyChannelSolo = false;

    const auto quarterNoteSamples = sampleRate * 60.0 / juce::jmax (20.0, bpm);
    const auto stepsPerBar = juce::jmax (1, pattern.numSteps / 4);
    snap->samplesPerStep = quarterNoteSamples / static_cast<double> (stepsPerBar);

    for (int row = 0; row < snap->numChannels; ++row)
    {
        const auto& patternRow = pattern.rows[static_cast<size_t> (row)];
        auto& channel = snap->channels[static_cast<size_t> (row)];

        if (const auto sampleId = sampleManager.resolvePlaybackSampleId (patternRow.assetId, patternRow.sliceIndex))
            channel.sampleId = *sampleId;

        channel.pitchSemitones = sampleManager.resolvePlaybackPitchSemitones (patternRow.assetId, patternRow.sliceIndex);
        channel.timeRatio = sampleManager.resolvePlaybackTimeRatio (patternRow.assetId, patternRow.sliceIndex);
        channel.loop = false;
        channel.mode = sampleManager.resolveVoicePlaybackMode (patternRow.assetId, patternRow.sliceIndex);
        channel.channelGain = patternRow.channelGain;
        channel.channelPan = patternRow.channelPan;
        channel.channelMute = patternRow.channelMute;
        channel.channelSolo = patternRow.channelSolo;

        if (patternRow.channelSolo)
            snap->anyChannelSolo = true;

        for (int s = 0; s < pattern.numSteps; ++s)
        {
            const auto& step = patternRow.steps[static_cast<size_t> (s)];
            auto& out = channel.steps[static_cast<size_t> (s)];
            out.active = step.active;
            out.velocity = step.velocity;
            out.probability = step.probability;
        }
    }

    return snap;
}

SharedNoteSnapshot PatternStore::buildNoteSnapshotForPattern (int patternIndex,
                                                              double bpm,
                                                              double sampleRate) const
{
    const auto& patterns = project.getPatterns();

    if (! juce::isPositiveAndBelow (patternIndex, static_cast<int> (patterns.size())))
        return std::make_shared<NoteSnapshot>();

    const auto& pattern = patterns[static_cast<size_t> (patternIndex)];
    auto snap = std::make_shared<NoteSnapshot>();

    const auto samplesPerBeat = sampleRate * 60.0 / juce::jmax (20.0, bpm);
    const auto loopBeats = static_cast<double> (pattern.lengthBars) * 4.0;
    snap->loopLengthSamples = static_cast<uint64_t> (loopBeats * samplesPerBeat);

    if (snap->loopLengthSamples == 0)
        return snap;

    snap->numNotes = juce::jmin (static_cast<int> (pattern.notes.size()), kMaxScheduledNotes);

    for (int i = 0; i < snap->numNotes; ++i)
    {
        const auto& note = pattern.notes[static_cast<size_t> (i)];
        auto& out = snap->notes[static_cast<size_t> (i)];

        const auto startSamples = static_cast<uint64_t> (note.startBeats * samplesPerBeat);
        out.startInPatternSamples = startSamples % snap->loopLengthSamples;
        out.velocity = note.velocity;

        if (const auto sampleId = sampleManager.resolvePlaybackSampleId (note.assetId, note.sliceIndex))
            out.sampleId = *sampleId;

        const auto slicePitch = sampleManager.resolvePlaybackPitchSemitones (note.assetId, note.sliceIndex);
        out.pitchSemitones = slicePitch + static_cast<float> (note.pitch - kPianoRollRootPitch);
        out.timeRatio = sampleManager.resolvePlaybackTimeRatio (note.assetId, note.sliceIndex);
        out.mode = sampleManager.resolveVoicePlaybackMode (note.assetId, note.sliceIndex);
    }

    return snap;
}

void PatternStore::publishSnapshot (AudioEngine& engine, double bpm, double sampleRate)
{
    publishSnapshotForPattern (getCurrentPatternIndex(), engine, bpm, sampleRate);
}

SharedFxSnapshot PatternStore::buildFxSnapshotForPattern (int patternIndex) const
{
    const auto& patterns = project.getPatterns();

    if (! juce::isPositiveAndBelow (patternIndex, static_cast<int> (patterns.size())))
        return std::make_shared<FxSnapshot>();

    const auto& pattern = patterns[static_cast<size_t> (patternIndex)];
    auto snap = std::make_shared<FxSnapshot>();
    snap->numChannels = juce::jmin (static_cast<int> (pattern.rows.size()), kMaxPatternChannels);

    for (int row = 0; row < snap->numChannels; ++row)
    {
        const auto& patternRow = pattern.rows[static_cast<size_t> (row)];
        auto& fx = snap->channels[static_cast<size_t> (row)];
        fx.eq = patternRow.channelEq;
        fx.color = patternRow.channelColor;
        fx.compressor = patternRow.channelCompressor;
        fx.delay = patternRow.channelDelay;
        fx.reverb = patternRow.channelReverb;
    }

    return snap;
}

void PatternStore::publishSnapshotForPattern (int patternIndex,
                                              AudioEngine& engine,
                                              double bpm,
                                              double sampleRate)
{
    engine.setPatternSnapshot (buildSnapshotForPattern (patternIndex, bpm, sampleRate));
    engine.setNoteSnapshot (buildNoteSnapshotForPattern (patternIndex, bpm, sampleRate));
    engine.setFxSnapshot (buildFxSnapshotForPattern (patternIndex));
}

int PatternStore::getCurrentStepForUi (const AudioEngine& engine) const noexcept
{
    return engine.getCurrentSequencerStep();
}

} // namespace sampr
