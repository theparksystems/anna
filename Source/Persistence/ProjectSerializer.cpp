#include "ProjectSerializer.h"

#include <algorithm>

namespace sampr
{

namespace
{
    juce::var sampleRefToVar (const SampleRefData& ref, const juce::File& projectRoot);

    juce::var stepToVar (const Step& step)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("active", step.active);
        obj->setProperty ("velocity", step.velocity);
        obj->setProperty ("probability", step.probability);
        return juce::var (obj);
    }

    bool stepFromVar (const juce::var& v, Step& stepOut)
    {
        if (! v.isObject())
            return false;

        stepOut.active = static_cast<bool> (v.getProperty ("active", false));
        stepOut.velocity = static_cast<float> (static_cast<double> (v.getProperty ("velocity", 1.0)));
        stepOut.probability = static_cast<float> (static_cast<double> (v.getProperty ("probability", 1.0)));
        return true;
    }

    juce::var noteToVar (const NoteEvent& note)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", static_cast<int> (note.id));
        obj->setProperty ("startBeats", note.startBeats);
        obj->setProperty ("durationBeats", note.durationBeats);
        obj->setProperty ("pitch", note.pitch);
        obj->setProperty ("velocity", note.velocity);
        obj->setProperty ("sampleRefId", juce::String (note.sampleRefId));
        obj->setProperty ("sliceIndex", note.sliceIndex);
        return juce::var (obj);
    }

    bool noteFromVar (const juce::var& v, NoteEvent& noteOut)
    {
        if (! v.isObject())
            return false;

        noteOut.id = static_cast<NoteId> (static_cast<int> (v.getProperty ("id", 0)));
        noteOut.startBeats = static_cast<double> (v.getProperty ("startBeats", 0.0));
        noteOut.durationBeats = static_cast<double> (v.getProperty ("durationBeats", 0.25));
        noteOut.pitch = static_cast<int> (v.getProperty ("pitch", kPianoRollRootPitch));
        noteOut.velocity = static_cast<float> (static_cast<double> (v.getProperty ("velocity", 0.85)));
        noteOut.sampleRefId = v.getProperty ("sampleRefId", juce::String()).toString();
        noteOut.sliceIndex = static_cast<int> (v.getProperty ("sliceIndex", 0));
        return true;
    }

    juce::var rowToVar (const PatternRow& row)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("sampleRefId", juce::String (row.sampleRefId));
        obj->setProperty ("sliceIndex", row.sliceIndex);
        obj->setProperty ("label", row.label);
        obj->setProperty ("colour", static_cast<int> (row.colour.getARGB()));
        obj->setProperty ("channelGain", row.channelGain);
        obj->setProperty ("channelPan", row.channelPan);
        obj->setProperty ("channelMute", row.channelMute);
        obj->setProperty ("channelSolo", row.channelSolo);

        auto* eq = new juce::DynamicObject();
        eq->setProperty ("enabled", row.channelEq.enabled);
        eq->setProperty ("lowFreq", row.channelEq.low.frequencyHz);
        eq->setProperty ("lowGain", row.channelEq.low.gainDb);
        eq->setProperty ("lowQ", row.channelEq.low.q);
        eq->setProperty ("midFreq", row.channelEq.mid.frequencyHz);
        eq->setProperty ("midGain", row.channelEq.mid.gainDb);
        eq->setProperty ("midQ", row.channelEq.mid.q);
        eq->setProperty ("highFreq", row.channelEq.high.frequencyHz);
        eq->setProperty ("highGain", row.channelEq.high.gainDb);
        eq->setProperty ("highQ", row.channelEq.high.q);
        obj->setProperty ("channelEq", juce::var (eq));

        auto* color = new juce::DynamicObject();
        color->setProperty ("enabled", row.channelColor.enabled);
        color->setProperty ("lowCutHz", row.channelColor.lowCutHz);
        color->setProperty ("highCutHz", row.channelColor.highCutHz);
        color->setProperty ("drive", row.channelColor.drive);
        color->setProperty ("width", row.channelColor.width);
        color->setProperty ("chorusMix", row.channelColor.chorusMix);
        color->setProperty ("chorusRateHz", row.channelColor.chorusRateHz);
        color->setProperty ("phaserMix", row.channelColor.phaserMix);
        color->setProperty ("phaserRateHz", row.channelColor.phaserRateHz);
        color->setProperty ("pump", row.channelColor.pump);
        color->setProperty ("limiterCeilingDb", row.channelColor.limiterCeilingDb);
        obj->setProperty ("channelColor", juce::var (color));

        auto* comp = new juce::DynamicObject();
        comp->setProperty ("enabled", row.channelCompressor.enabled);
        comp->setProperty ("thresholdDb", row.channelCompressor.thresholdDb);
        comp->setProperty ("ratio", row.channelCompressor.ratio);
        comp->setProperty ("attackMs", row.channelCompressor.attackMs);
        comp->setProperty ("releaseMs", row.channelCompressor.releaseMs);
        comp->setProperty ("makeupGainDb", row.channelCompressor.makeupGainDb);
        obj->setProperty ("channelCompressor", juce::var (comp));

        auto* delay = new juce::DynamicObject();
        delay->setProperty ("enabled", row.channelDelay.enabled);
        delay->setProperty ("timeMs", row.channelDelay.timeMs);
        delay->setProperty ("feedback", row.channelDelay.feedback);
        delay->setProperty ("mix", row.channelDelay.mix);
        obj->setProperty ("channelDelay", juce::var (delay));

        auto* reverb = new juce::DynamicObject();
        reverb->setProperty ("enabled", row.channelReverb.enabled);
        reverb->setProperty ("roomSize", row.channelReverb.roomSize);
        reverb->setProperty ("damping", row.channelReverb.damping);
        reverb->setProperty ("wetLevel", row.channelReverb.wetLevel);
        reverb->setProperty ("dryLevel", row.channelReverb.dryLevel);
        reverb->setProperty ("width", row.channelReverb.width);
        obj->setProperty ("channelReverb", juce::var (reverb));

        juce::Array<juce::var> steps;

        for (const auto& step : row.steps)
            steps.add (stepToVar (step));

        obj->setProperty ("steps", steps);
        return juce::var (obj);
    }

    bool rowFromVar (const juce::var& v, PatternRow& rowOut)
    {
        if (! v.isObject())
            return false;

        rowOut.sampleRefId = v.getProperty ("sampleRefId", juce::String()).toString();
        rowOut.sliceIndex = static_cast<int> (v.getProperty ("sliceIndex", 0));
        rowOut.label = v.getProperty ("label", juce::String()).toString();
        rowOut.colour = juce::Colour (static_cast<juce::uint32> (
            static_cast<int> (v.getProperty ("colour", juce::var (static_cast<int> (0xff4cc2ff))))));
        rowOut.channelGain = static_cast<float> (static_cast<double> (v.getProperty ("channelGain", 1.0)));
        rowOut.channelPan = static_cast<float> (static_cast<double> (v.getProperty ("channelPan", 0.0)));
        rowOut.channelMute = static_cast<bool> (v.getProperty ("channelMute", false));
        rowOut.channelSolo = static_cast<bool> (v.getProperty ("channelSolo", false));

        if (const auto eq = v.getProperty ("channelEq", juce::var()); eq.isObject())
        {
            rowOut.channelEq.enabled = static_cast<bool> (eq.getProperty ("enabled", true));
            rowOut.channelEq.low.frequencyHz = static_cast<float> (static_cast<double> (eq.getProperty ("lowFreq", 120.0)));
            rowOut.channelEq.low.gainDb = static_cast<float> (static_cast<double> (eq.getProperty ("lowGain", 0.0)));
            rowOut.channelEq.low.q = static_cast<float> (static_cast<double> (eq.getProperty ("lowQ", 0.707)));
            rowOut.channelEq.mid.frequencyHz = static_cast<float> (static_cast<double> (eq.getProperty ("midFreq", 1000.0)));
            rowOut.channelEq.mid.gainDb = static_cast<float> (static_cast<double> (eq.getProperty ("midGain", 0.0)));
            rowOut.channelEq.mid.q = static_cast<float> (static_cast<double> (eq.getProperty ("midQ", 1.0)));
            rowOut.channelEq.high.frequencyHz = static_cast<float> (static_cast<double> (eq.getProperty ("highFreq", 8000.0)));
            rowOut.channelEq.high.gainDb = static_cast<float> (static_cast<double> (eq.getProperty ("highGain", 0.0)));
            rowOut.channelEq.high.q = static_cast<float> (static_cast<double> (eq.getProperty ("highQ", 0.707)));
        }

        if (const auto comp = v.getProperty ("channelCompressor", juce::var()); comp.isObject())
        {
            rowOut.channelCompressor.enabled = static_cast<bool> (comp.getProperty ("enabled", false));
            rowOut.channelCompressor.thresholdDb = static_cast<float> (static_cast<double> (comp.getProperty ("thresholdDb", -18.0)));
            rowOut.channelCompressor.ratio = static_cast<float> (static_cast<double> (comp.getProperty ("ratio", 4.0)));
            rowOut.channelCompressor.attackMs = static_cast<float> (static_cast<double> (comp.getProperty ("attackMs", 10.0)));
            rowOut.channelCompressor.releaseMs = static_cast<float> (static_cast<double> (comp.getProperty ("releaseMs", 100.0)));
            rowOut.channelCompressor.makeupGainDb = static_cast<float> (static_cast<double> (comp.getProperty ("makeupGainDb", 0.0)));
        }

        if (const auto color = v.getProperty ("channelColor", juce::var()); color.isObject())
        {
            rowOut.channelColor.enabled = static_cast<bool> (color.getProperty ("enabled", false));
            rowOut.channelColor.lowCutHz = static_cast<float> (static_cast<double> (color.getProperty ("lowCutHz", 20.0)));
            rowOut.channelColor.highCutHz = static_cast<float> (static_cast<double> (color.getProperty ("highCutHz", 20000.0)));
            rowOut.channelColor.drive = static_cast<float> (static_cast<double> (color.getProperty ("drive", 0.0)));
            rowOut.channelColor.width = static_cast<float> (static_cast<double> (color.getProperty ("width", 1.0)));
            rowOut.channelColor.chorusMix = static_cast<float> (static_cast<double> (color.getProperty ("chorusMix", 0.0)));
            rowOut.channelColor.chorusRateHz = static_cast<float> (static_cast<double> (color.getProperty ("chorusRateHz", 0.35)));
            rowOut.channelColor.phaserMix = static_cast<float> (static_cast<double> (color.getProperty ("phaserMix", 0.0)));
            rowOut.channelColor.phaserRateHz = static_cast<float> (static_cast<double> (color.getProperty ("phaserRateHz", 0.25)));
            rowOut.channelColor.pump = static_cast<float> (static_cast<double> (color.getProperty ("pump", 0.0)));
            rowOut.channelColor.limiterCeilingDb = static_cast<float> (static_cast<double> (color.getProperty ("limiterCeilingDb", -1.0)));
        }

        if (const auto delay = v.getProperty ("channelDelay", juce::var()); delay.isObject())
        {
            rowOut.channelDelay.enabled = static_cast<bool> (delay.getProperty ("enabled", false));
            rowOut.channelDelay.timeMs = static_cast<float> (static_cast<double> (delay.getProperty ("timeMs", 250.0)));
            rowOut.channelDelay.feedback = static_cast<float> (static_cast<double> (delay.getProperty ("feedback", 0.35)));
            rowOut.channelDelay.mix = static_cast<float> (static_cast<double> (delay.getProperty ("mix", 0.25)));
        }

        if (const auto reverb = v.getProperty ("channelReverb", juce::var()); reverb.isObject())
        {
            rowOut.channelReverb.enabled = static_cast<bool> (reverb.getProperty ("enabled", false));
            rowOut.channelReverb.roomSize = static_cast<float> (static_cast<double> (reverb.getProperty ("roomSize", 0.5)));
            rowOut.channelReverb.damping = static_cast<float> (static_cast<double> (reverb.getProperty ("damping", 0.5)));
            rowOut.channelReverb.wetLevel = static_cast<float> (static_cast<double> (reverb.getProperty ("wetLevel", 0.25)));
            rowOut.channelReverb.dryLevel = static_cast<float> (static_cast<double> (reverb.getProperty ("dryLevel", 0.8)));
            rowOut.channelReverb.width = static_cast<float> (static_cast<double> (reverb.getProperty ("width", 1.0)));
        }

        rowOut.steps.clear();

        if (const auto* steps = v.getProperty ("steps", juce::var()).getArray())
        {
            for (const auto& stepVar : *steps)
            {
                Step step;
                stepFromVar (stepVar, step);
                rowOut.steps.push_back (step);
            }
        }

        return true;
    }

    juce::var patternToVar (const Pattern& pattern)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", static_cast<int> (pattern.id));
        obj->setProperty ("name", pattern.name);
        obj->setProperty ("numSteps", pattern.numSteps);
        obj->setProperty ("lengthBars", pattern.lengthBars);

        juce::Array<juce::var> rows;

        for (const auto& row : pattern.rows)
            rows.add (rowToVar (row));

        juce::Array<juce::var> notes;

        for (const auto& note : pattern.notes)
            notes.add (noteToVar (note));

        obj->setProperty ("rows", rows);
        obj->setProperty ("notes", notes);
        return juce::var (obj);
    }

    bool patternFromVar (const juce::var& v, Pattern& patternOut)
    {
        if (! v.isObject())
            return false;

        patternOut.id = static_cast<PatternId> (static_cast<int> (v.getProperty ("id", 0)));
        patternOut.name = v.getProperty ("name", juce::String ("Pattern")).toString();
        patternOut.numSteps = static_cast<int> (v.getProperty ("numSteps", 16));
        patternOut.lengthBars = static_cast<int> (v.getProperty ("lengthBars", 1));
        patternOut.rows.clear();
        patternOut.notes.clear();

        if (const auto* rows = v.getProperty ("rows", juce::var()).getArray())
        {
            for (const auto& rowVar : *rows)
            {
                PatternRow row;
                rowFromVar (rowVar, row);
                patternOut.rows.push_back (std::move (row));
            }
        }

        if (const auto* notes = v.getProperty ("notes", juce::var()).getArray())
        {
            for (const auto& noteVar : *notes)
            {
                NoteEvent note;
                noteFromVar (noteVar, note);
                patternOut.notes.push_back (std::move (note));
            }
        }

        return true;
    }

    juce::var sliceRefToVar (const SliceRefData& slice)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", static_cast<int> (slice.id));
        obj->setProperty ("startSample", slice.startSample);
        obj->setProperty ("endSample", slice.endSample);
        obj->setProperty ("pitchSemitones", slice.pitchSemitones);
        obj->setProperty ("timeRatio", slice.timeRatio);
        obj->setProperty ("loop", slice.loop);
        obj->setProperty ("processingMode", static_cast<int> (slice.processingMode));
        return juce::var (obj);
    }

    bool sliceRefFromVar (const juce::var& v, SliceRefData& sliceOut)
    {
        if (! v.isObject())
            return false;

        sliceOut.id = static_cast<SliceId> (static_cast<int> (v.getProperty ("id", 0)));
        sliceOut.startSample = static_cast<int> (v.getProperty ("startSample", 0));
        sliceOut.endSample = static_cast<int> (v.getProperty ("endSample", 0));
        sliceOut.pitchSemitones = static_cast<float> (static_cast<double> (v.getProperty ("pitchSemitones", 0.0)));
        sliceOut.timeRatio = static_cast<float> (static_cast<double> (v.getProperty ("timeRatio", 1.0)));
        sliceOut.loop = static_cast<bool> (v.getProperty ("loop", false));
        sliceOut.processingMode = static_cast<StretchProcessingMode> (
            static_cast<int> (v.getProperty ("processingMode", 0)));
        return true;
    }

    juce::var originToVar (const SampleOriginMetadata& origin)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("sourceType", origin.sourceType);
        obj->setProperty ("sourceUrl", origin.sourceUrl);
        obj->setProperty ("sourceTitle", origin.sourceTitle);
        obj->setProperty ("sourceAuthor", origin.sourceAuthor);
        obj->setProperty ("sourceId", origin.sourceId);
        obj->setProperty ("downloadedAt", origin.downloadedAt);
        obj->setProperty ("localFileName", origin.localFileName);
        return juce::var (obj);
    }

    SampleOriginMetadata originFromVar (const juce::var& v)
    {
        SampleOriginMetadata origin;

        if (! v.isObject())
            return origin;

        origin.sourceType = v.getProperty ("sourceType", juce::String()).toString();
        origin.sourceUrl = v.getProperty ("sourceUrl", juce::String()).toString();
        origin.sourceTitle = v.getProperty ("sourceTitle", juce::String()).toString();
        origin.sourceAuthor = v.getProperty ("sourceAuthor", juce::String()).toString();
        origin.sourceId = v.getProperty ("sourceId", juce::String()).toString();
        origin.downloadedAt = v.getProperty ("downloadedAt", juce::String()).toString();
        origin.localFileName = v.getProperty ("localFileName", juce::String()).toString();
        return origin;
    }

    void writeSampleMetadataFiles (const ProjectModel& model,
                                   const juce::File& projectRoot,
                                   juce::String& errorOut)
    {
        const auto metadataDir = projectRoot.getChildFile ("metadata");

        if (! metadataDir.exists() && ! metadataDir.createDirectory())
        {
            errorOut = "Could not create metadata directory.";
            return;
        }

        juce::Array<juce::var> sampleSources;
        juce::String sampledFrom;

        for (const auto& ref : model.getSampleRefs())
        {
            auto sampleVar = sampleRefToVar (ref, projectRoot);
            sampleSources.add (sampleVar);

            if (ref.origin.hasSource())
            {
                const auto title = ref.origin.sourceTitle.isNotEmpty() ? ref.origin.sourceTitle : ref.displayName;
                sampledFrom << "- " << title;

                if (ref.origin.sourceAuthor.isNotEmpty())
                    sampledFrom << " by " << ref.origin.sourceAuthor;

                if (ref.origin.sourceUrl.isNotEmpty())
                    sampledFrom << " - " << ref.origin.sourceUrl;

                sampledFrom << "\n";
            }
        }

        if (! metadataDir.getChildFile ("sample_sources.json").replaceWithText (juce::JSON::toString (sampleSources, true)))
            errorOut = "Could not write metadata/sample_sources.json.";

        if (errorOut.isEmpty()
            && ! metadataDir.getChildFile ("sampled_from.txt").replaceWithText (sampledFrom))
            errorOut = "Could not write metadata/sampled_from.txt.";
    }

    juce::var sampleRefToVar (const SampleRefData& ref, const juce::File& projectRoot)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("refId", ref.refId);
        obj->setProperty ("displayName", ref.displayName);
        obj->setProperty ("selectedSliceIndex", ref.selectedSliceIndex);
        obj->setProperty ("origin", originToVar (ref.origin));

        juce::File relative;

        if (ref.relativePath.isNotEmpty())
            relative = juce::File (ref.relativePath);

        if (juce::File::isAbsolutePath (relative.getFullPathName()) && projectRoot.exists())
            relative = projectRoot.getChildFile (relative.getFileName());

        obj->setProperty ("relativePath", relative.getRelativePathFrom (projectRoot));

        juce::Array<juce::var> slices;

        for (const auto& slice : ref.slices)
            slices.add (sliceRefToVar (slice));

        obj->setProperty ("slices", slices);
        return juce::var (obj);
    }

    bool sampleRefFromVar (const juce::var& v, SampleRefData& refOut)
    {
        if (! v.isObject())
            return false;

        refOut.refId = v.getProperty ("refId", juce::String()).toString();
        refOut.relativePath = v.getProperty ("relativePath", juce::String()).toString();
        refOut.displayName = v.getProperty ("displayName", juce::String()).toString();
        refOut.selectedSliceIndex = static_cast<int> (v.getProperty ("selectedSliceIndex", 0));
        refOut.origin = originFromVar (v.getProperty ("origin", juce::var()));
        refOut.slices.clear();

        if (const auto* slices = v.getProperty ("slices", juce::var()).getArray())
        {
            for (const auto& sliceVar : *slices)
            {
                SliceRefData slice;
                sliceRefFromVar (sliceVar, slice);
                refOut.slices.push_back (std::move (slice));
            }
        }

        return true;
    }

    juce::var clipToVar (const ArrangementClip& clip)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", static_cast<int> (clip.id));
        obj->setProperty ("type", static_cast<int> (clip.type));
        obj->setProperty ("patternId", static_cast<int> (clip.patternId));
        obj->setProperty ("startBar", clip.startBar);
        obj->setProperty ("lengthBars", clip.lengthBars);
        obj->setProperty ("name", clip.name);
        obj->setProperty ("colour", static_cast<int> (clip.colour.getARGB()));
        return juce::var (obj);
    }

    bool clipFromVar (const juce::var& v, ArrangementClip& clipOut)
    {
        if (! v.isObject())
            return false;

        clipOut.id = static_cast<ClipId> (static_cast<int> (v.getProperty ("id", 0)));
        clipOut.type = static_cast<ClipType> (static_cast<int> (v.getProperty ("type", 0)));
        clipOut.patternId = static_cast<PatternId> (static_cast<int> (v.getProperty ("patternId", 0)));
        clipOut.startBar = static_cast<double> (v.getProperty ("startBar", 0.0));
        clipOut.lengthBars = static_cast<int> (v.getProperty ("lengthBars", 4));
        clipOut.name = v.getProperty ("name", juce::String()).toString();
        clipOut.colour = juce::Colour (static_cast<juce::uint32> (
            static_cast<int> (v.getProperty ("colour", juce::var (static_cast<int> (0xff4cc2ff))))));
        return true;
    }

    juce::String playbackModeToString (PlaybackMode mode)
    {
        return mode == PlaybackMode::song ? "song" : "pattern";
    }

    PlaybackMode playbackModeFromString (const juce::String& text)
    {
        return text == "song" ? PlaybackMode::song : PlaybackMode::pattern;
    }
}

bool ProjectSerializer::saveToFile (const ProjectModel& model,
                                   const juce::File& projectFile,
                                   juce::String& errorOut)
{
    const auto parent = projectFile.getParentDirectory();

    if (! parent.exists())
    {
        if (! parent.createDirectory())
        {
            errorOut = "Could not create project directory.";
            return false;
        }
    }

    const auto json = juce::JSON::toString (toVar (model, parent), true);

    if (! projectFile.replaceWithText (json))
    {
        errorOut = "Could not write project file.";
        return false;
    }

    writeSampleMetadataFiles (model, parent, errorOut);

    if (errorOut.isNotEmpty())
        return false;

    return true;
}

ProjectLoadResult ProjectSerializer::loadFromFile (const juce::File& projectFile)
{
    ProjectLoadResult result;
    result.projectFile = projectFile;

    if (! projectFile.existsAsFile())
    {
        result.errorMessage = "Project file does not exist.";
        return result;
    }

    juce::var parsed;
    const auto parseError = juce::JSON::parse (projectFile.loadFileAsString(), parsed);

    if (parseError.failed())
    {
        result.errorMessage = parseError.getErrorMessage();
        return result;
    }

    juce::String modelError;

    if (! fromVar (parsed, result.model, modelError))
    {
        result.errorMessage = modelError;
        return result;
    }

    result.success = true;
    return result;
}

juce::var ProjectSerializer::toVar (const ProjectModel& model, const juce::File& projectRoot)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", kFormatId);
    root->setProperty ("version", kFormatVersion);

    auto* settings = new juce::DynamicObject();
    const auto& s = model.getSettings();
    settings->setProperty ("projectName", s.projectName);
    settings->setProperty ("bpm", s.bpm);
    settings->setProperty ("masterGain", s.masterGain);
    settings->setProperty ("arrangementLengthBars", s.arrangementLengthBars);
    settings->setProperty ("playbackMode", playbackModeToString (s.playbackMode));
    settings->setProperty ("currentPatternIndex", s.currentPatternIndex);
    settings->setProperty ("metronomeEnabled", s.metronomeEnabled);
    settings->setProperty ("loopEnabled", s.loopEnabled);
    settings->setProperty ("loopStartBeats", s.loopStartBeats);
    settings->setProperty ("loopEndBeats", s.loopEndBeats);
    root->setProperty ("settings", juce::var (settings));

    juce::Array<juce::var> samples;

    for (const auto& ref : model.getSampleRefs())
        samples.add (sampleRefToVar (ref, projectRoot));

    root->setProperty ("samples", samples);

    juce::Array<juce::var> patterns;

    for (const auto& pattern : model.getPatterns())
        patterns.add (patternToVar (pattern));

    root->setProperty ("patterns", patterns);

    juce::Array<juce::var> arrangement;

    for (const auto& clip : model.getArrangement())
        arrangement.add (clipToVar (clip));

    root->setProperty ("arrangement", arrangement);

    auto* ids = new juce::DynamicObject();
    ids->setProperty ("nextPatternId", static_cast<int> (model.getNextPatternId()));
    ids->setProperty ("nextNoteId", static_cast<int> (model.getNextNoteId()));
    ids->setProperty ("nextClipId", static_cast<int> (model.getNextClipId()));
    root->setProperty ("idState", juce::var (ids));

    return juce::var (root);
}

bool ProjectSerializer::fromVar (const juce::var& root, ProjectModel& modelOut, juce::String& errorOut)
{
    if (! root.isObject())
    {
        errorOut = "Invalid project root.";
        return false;
    }

    const auto format = root.getProperty ("format", juce::String()).toString();
    if (format != kFormatId && format != kLegacyFormatId)
    {
        errorOut = "Unrecognized project format.";
        return false;
    }

    const auto version = static_cast<int> (root.getProperty ("version", 0));

    if (version != kFormatVersion)
    {
        errorOut = "Unsupported project version.";
        return false;
    }

    ProjectModel loaded;
    auto& settings = loaded.getSettings();

    if (const auto settingsVar = root.getProperty ("settings", juce::var()); settingsVar.isObject())
    {
        settings.projectName = settingsVar.getProperty ("projectName", juce::String ("Untitled")).toString();
        settings.bpm = static_cast<double> (settingsVar.getProperty ("bpm", 120.0));
        settings.masterGain = static_cast<float> (static_cast<double> (settingsVar.getProperty ("masterGain", 1.0)));
        settings.arrangementLengthBars = static_cast<int> (settingsVar.getProperty ("arrangementLengthBars", 64));
        settings.playbackMode = playbackModeFromString (
            settingsVar.getProperty ("playbackMode", "pattern").toString());
        settings.currentPatternIndex = static_cast<int> (settingsVar.getProperty ("currentPatternIndex", 0));
        settings.metronomeEnabled = static_cast<bool> (settingsVar.getProperty ("metronomeEnabled", false));
        settings.loopEnabled = static_cast<bool> (settingsVar.getProperty ("loopEnabled", false));
        settings.loopStartBeats = static_cast<double> (settingsVar.getProperty ("loopStartBeats", 0.0));
        settings.loopEndBeats = static_cast<double> (settingsVar.getProperty ("loopEndBeats", 16.0));
    }

    if (const auto* samples = root.getProperty ("samples", juce::var()).getArray())
    {
        for (const auto& sampleVar : *samples)
        {
            SampleRefData ref;
            sampleRefFromVar (sampleVar, ref);
            loaded.getSampleRefs().push_back (std::move (ref));
        }
    }

    if (const auto* patterns = root.getProperty ("patterns", juce::var()).getArray())
    {
        loaded.getPatterns().clear();

        for (const auto& patternVar : *patterns)
        {
            Pattern pattern;
            patternFromVar (patternVar, pattern);
            loaded.getPatterns().push_back (std::move (pattern));
        }
    }

    if (loaded.getPatterns().empty())
    {
        errorOut = "Project contains no patterns.";
        return false;
    }

    if (const auto* arrangement = root.getProperty ("arrangement", juce::var()).getArray())
    {
        for (const auto& clipVar : *arrangement)
        {
            ArrangementClip clip;
            clipFromVar (clipVar, clip);
            loaded.getArrangement().push_back (std::move (clip));
        }
    }

    loaded.syncIdGeneratorsFromContent();

    if (! juce::isPositiveAndBelow (settings.currentPatternIndex,
                                    static_cast<int> (loaded.getPatterns().size())))
        settings.currentPatternIndex = 0;

    modelOut.assignFrom (loaded);
    return true;
}

juce::var ProjectSerializer::serializePattern (const Pattern& pattern)
{
    return patternToVar (pattern);
}

bool ProjectSerializer::deserializePattern (const juce::var& root, Pattern& patternOut)
{
    return patternFromVar (root, patternOut);
}

} // namespace sampr
