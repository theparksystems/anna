#include <cmath>
#include <iostream>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "../Source/Audio/AudioTypes.h"
#include "../Source/DSP/OnsetDetector.h"
#include "../Source/DSP/PeakGenerator.h"
#include "../Source/DSP/RubberBandOfflineProcessor.h"
#include "../Source/DSP/RubberBandStreamProcessor.h"
#include "../Source/Model/Pattern.h"
#include "../Source/Model/ProjectModel.h"
#include "../Source/Model/SampleAsset.h"
#include "../Source/Assistant/TrackContextBuilder.h"
#include "../Source/Persistence/BeatSerializer.h"
#include "../Source/Persistence/ProjectSerializer.h"

namespace
{
    int failures = 0;

    void expect (bool condition, const char* message)
    {
        if (! condition)
        {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void testSemitonesToRatio()
    {
        expect (std::abs (sampr::semitonesToRatio (0.0f) - 1.0f) < 0.0001f, "0 semitones = unity ratio");
        expect (std::abs (sampr::semitonesToRatio (12.0f) - 2.0f) < 0.0001f, "12 semitones = 2x ratio");
    }

    void testPeakGenerator()
    {
        juce::AudioBuffer<float> buffer (1, 128);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, std::sin (static_cast<float> (i) * 0.1f));

        const auto peaks = sampr::PeakGenerator::generate (buffer, 32);
        expect (peaks.numColumns > 0, "peak columns generated");
        expect (static_cast<int> (peaks.minMax.size()) == peaks.numColumns * 2, "peak min/max size");
    }

    void testProjectRoundTrip()
    {
        sampr::ProjectModel original;
        original.getSettings().projectName = "Test Project";
        original.getSettings().bpm = 140.0;
        auto& pattern = original.getPatterns().front();
        pattern.name = "Drums";
        pattern.rows.push_back ({});
        pattern.rows.back().label = "Kick";
        pattern.rows.back().channelGain = 0.8f;

        const auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("sampr_test_project.sampr.json");

        juce::String saveError;
        expect (sampr::ProjectSerializer::saveToFile (original, tempFile, saveError),
                "project saves");

        const auto loaded = sampr::ProjectSerializer::loadFromFile (tempFile);
        expect (loaded.success, "project loads");
        expect (loaded.model.getSettings().bpm == 140.0, "bpm round-trips");
        expect (loaded.model.getPatterns().front().name == "Drums", "pattern name round-trips");
        expect (loaded.model.getPatterns().front().rows.front().channelGain == 0.8f,
                "mixer gain round-trips");

        tempFile.deleteFile();
    }

    void testFxRoundTrip()
    {
        sampr::ProjectModel original;
        auto& pattern = original.getPatterns().front();
        pattern.rows.push_back ({});
        auto& row = pattern.rows.back();
        row.label = "Snare";
        row.channelEq.enabled = true;
        row.channelEq.mid.gainDb = 3.5f;
        row.channelCompressor.enabled = true;
        row.channelCompressor.thresholdDb = -24.0f;
        row.channelCompressor.ratio = 6.0f;
        row.channelDelay.enabled = true;
        row.channelDelay.timeMs = 180.0f;
        row.channelReverb.enabled = true;
        row.channelReverb.roomSize = 0.7f;

        const auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("sampr_test_fx.sampr.json");

        juce::String saveError;
        expect (sampr::ProjectSerializer::saveToFile (original, tempFile, saveError),
                "fx project saves");

        const auto loaded = sampr::ProjectSerializer::loadFromFile (tempFile);
        expect (loaded.success, "fx project loads");

        const auto& loadedRow = loaded.model.getPatterns().front().rows.front();
        expect (loadedRow.channelEq.mid.gainDb == 3.5f, "eq gain round-trips");
        expect (loadedRow.channelCompressor.ratio == 6.0f, "compressor ratio round-trips");
        expect (loadedRow.channelDelay.timeMs == 180.0f, "delay time round-trips");
        expect (loadedRow.channelReverb.roomSize == 0.7f, "reverb room round-trips");

        tempFile.deleteFile();
    }

    void testBeatRoundTrip()
    {
        sampr::Pattern pattern;
        pattern.name = "Trap Beat";
        pattern.numSteps = 16;
        pattern.lengthBars = 2;
        pattern.rows.push_back ({});
        pattern.rows.back().label = "Hat";
        pattern.rows.back().steps.assign (16, sampr::Step { true, 0.8f, 1.0f });
        pattern.rows.back().channelCompressor.enabled = true;
        pattern.rows.back().channelDelay.mix = 0.4f;

        const auto tempFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                  .getChildFile ("sampr_test_beat.sampr-beat.json");

        juce::String saveError;
        expect (sampr::BeatSerializer::saveToFile (pattern, tempFile, saveError),
                "beat saves");

        const auto loaded = sampr::BeatSerializer::loadFromFile (tempFile);
        expect (loaded.success, "beat loads");
        expect (loaded.pattern.name == "Trap Beat", "beat name round-trips");
        expect (loaded.pattern.rows.size() == 1, "beat row count round-trips");
        expect (loaded.pattern.rows.front().steps.front().active, "beat step round-trips");
        expect (loaded.pattern.rows.front().channelDelay.mix == 0.4f, "beat fx round-trips");

        tempFile.deleteFile();
    }

    void testProjectModelIds()
    {
        sampr::ProjectModel model;
        const auto p1 = model.allocatePatternId();
        const auto p2 = model.allocatePatternId();
        expect (p2 > p1, "pattern ids monotonic");
    }

    void testOnsetDetector()
    {
        juce::AudioBuffer<float> buffer (1, 44100);
        buffer.clear();

        const int impulseSpacing = 2205;
        const int impulseWidth = 64;

        for (int n = 0; n < 8; ++n)
        {
            const auto start = n * impulseSpacing;

        for (int i = 0; i < impulseWidth; ++i)
        {
            const auto sample = start + i;

            if (sample < buffer.getNumSamples())
                buffer.setSample (0, sample, std::sin (static_cast<float> (i) * 0.35f));
        }
        }

        sampr::OnsetDetectorOptions options;
        options.sensitivity = 1.0f;
        options.minGapSeconds = 0.03;

        const auto onsets = sampr::OnsetDetector::detect (buffer, 44100.0, options);
        expect (onsets.size() >= 3, "onset detector finds multiple transients");
        expect (onsets.front() > 0, "first onset is not at zero");
    }

    void testStretchModesAvailable()
    {
        expect (sampr::RubberBandOfflineProcessor::isAvailable(), "offline rubber band linked");
        expect (sampr::RubberBandStreamProcessor::isAvailable(), "stream rubber band linked");
    }

    void testProcessingModeEnum()
    {
        sampr::SliceRegion slice;
        slice.processingMode = sampr::StretchProcessingMode::realTimeStream;
        expect (static_cast<int> (slice.processingMode) == 2, "realTimeStream enum value");
    }

    void testTrackContextBuilder()
    {
        sampr::ProjectModel project;
        auto& pattern = project.getPatterns().front();
        pattern.name = "Test Beat";
        pattern.numSteps = 16;

        sampr::PatternRow row;
        row.label = "Snare";
        row.channelGain = 0.75f;
        row.channelCompressor.enabled = true;
        row.channelCompressor.ratio = 3.0f;
        row.steps.resize (16);

        for (int i = 0; i < 16; ++i)
        {
            row.steps[static_cast<size_t> (i)].active = (i % 4 == 0);
            row.steps[static_cast<size_t> (i)].velocity = 0.6f + static_cast<float> (i) * 0.02f;
        }

        pattern.rows.push_back (row);

        sampr::TrackContextInput input;
        input.scope = sampr::ContextScope::channel;
        input.project = &project;
        input.pattern = &pattern;
        input.patternIndex = 0;
        input.channelIndex = 0;
        input.bpm = 128.0;

        const auto json = sampr::TrackContextBuilder::build (input);
        expect (json.isObject(), "track context is object");
        expect (json.getProperty ("scope", juce::String()).toString() == "channel", "scope is channel");

        const auto channelVar = json.getProperty ("channel", juce::var());
        const auto* channel = channelVar.getDynamicObject();
        expect (channel != nullptr, "channel object exists");
        expect (static_cast<double> (channel->getProperty ("gain")) == 0.75, "channel gain");
        expect (channel->getProperty ("compressor").getDynamicObject() != nullptr,
                "compressor nested");

        const auto stepsVar = channel->getProperty ("steps");
        const auto* steps = stepsVar.getDynamicObject();
        expect (steps != nullptr, "step stats exist");
        expect (static_cast<int> (steps->getProperty ("activeSteps")) == 4, "active step count");
    }
}

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    testSemitonesToRatio();
    testPeakGenerator();
    testProjectRoundTrip();
    testFxRoundTrip();
    testBeatRoundTrip();
    testProjectModelIds();
    testOnsetDetector();
    testStretchModesAvailable();
    testProcessingModeEnum();
    testTrackContextBuilder();

    if (failures == 0)
    {
        std::cout << "All SAMPR unit tests passed.\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed.\n";
    return 1;
}