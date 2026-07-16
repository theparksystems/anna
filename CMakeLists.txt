cmake_minimum_required(VERSION 3.22)

project(ANNA VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# JUCE
# -----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------------
# Rubber Band Library (optional but recommended)
# Disable with: cmake -B build -DANNA_ENABLE_RUBBERBAND=OFF
# -----------------------------------------------------------------------------
option(ANNA_ENABLE_RUBBERBAND "Build with Rubber Band time/pitch DSP" ON)

if(ANNA_ENABLE_RUBBERBAND)
    include(cmake/FetchRubberBand.cmake)
endif()

option(ANNA_ENABLE_TRACKTION_ENGINE "Integrate Tracktion Engine module" OFF)
option(ANNA_BUILD_TESTS "Build ANNA unit test console app" ON)

if(ANNA_ENABLE_TRACKTION_ENGINE)
    include(cmake/OptionalTracktionEngine.cmake)
endif()

# -----------------------------------------------------------------------------
# Standalone GUI application
# -----------------------------------------------------------------------------
juce_add_gui_app(ANNA
    PRODUCT_NAME                "ANNA"
    COMPANY_NAME                "ANNA"
    BUNDLE_ID                   "com.anna.app"
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "ANNA needs microphone access for audio input."
)

juce_generate_juce_header(ANNA)

# Single native desktop entry point: Source/Shell/Application.cpp (START_JUCE_APPLICATION)
target_sources(ANNA
    PRIVATE
        Source/Shell/Application.cpp
        Source/Shell/MainComponent.cpp
        Source/App/MvpFlow.cpp
        Source/App/SampleImportService.cpp
        Source/Assistant/AssistantClient.cpp
        Source/Assistant/TrackContextBuilder.cpp
        Source/Audio/AudioCommandQueue.cpp
        Source/Audio/AudioDiagnostics.cpp
        Source/Audio/AudioEngine.cpp
        Source/Audio/ChannelMixer.cpp
        Source/Audio/LevelMeterBank.cpp
        Source/Audio/OfflineExporter.cpp
        Source/Audio/Mixer.cpp
        Source/Audio/NoteScheduler.cpp
        Source/Audio/PatternScheduler.cpp
        Source/Audio/SampleRegistry.cpp
        Source/Audio/SampleVoice.cpp
        Source/Audio/Transport.cpp
        Source/Audio/VoicePool.cpp
        Source/DSP/OnsetDetector.cpp
        Source/DSP/PeakGenerator.cpp
        Source/DSP/RubberBandOfflineProcessor.cpp
        Source/DSP/RubberBandStreamProcessor.cpp
        Source/DSP/StretchBakeWorker.cpp
        Source/Model/MixerState.cpp
        Source/Model/PatternStore.cpp
        Source/Model/ProjectController.cpp
        Source/Model/ProjectModel.cpp
        Source/Model/SampleManager.cpp
        Source/Model/UndoHistory.cpp
        Source/Persistence/BeatSerializer.cpp
        Source/Persistence/ProjectSerializer.cpp
        Source/Plugins/ColorProcessor.cpp
        Source/Plugins/CompressorProcessor.cpp
        Source/Plugins/DelayProcessor.cpp
        Source/Plugins/ParametricEqProcessor.cpp
        Source/Plugins/ReverbProcessor.cpp
        Source/UI/ArrangementTimelineComponent.cpp
        Source/UI/AssistantPanel.cpp
        Source/UI/ColorEditorPanel.cpp
        Source/UI/CompressorEditorPanel.cpp
        Source/UI/DelayEditorPanel.cpp
        Source/UI/EqEditorPanel.cpp
        Source/UI/FxWorkspaceComponent.cpp
        Source/UI/ReverbEditorPanel.cpp
        Source/UI/SamprLookAndFeel.cpp
        Source/UI/TopNavBarComponent.cpp
        Source/UI/MixerComponent.cpp
        Source/UI/TransportBarComponent.cpp
        Source/UI/PianoRollComponent.cpp
        Source/Patches/StepSequencerComponent.cpp
        Source/UI/SampleBrowserComponent.cpp
        Source/Patches/SliceEditorPanel.cpp
        Source/UI/WaveformComponent.cpp
)

target_compile_definitions(ANNA
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_MP3AUDIOFORMAT=1
        $<$<BOOL:${ANNA_ENABLE_RUBBERBAND}>:ANNA_HAS_RUBBERBAND=1>
)

target_link_libraries(ANNA
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(ANNA_ENABLE_RUBBERBAND)
    target_link_libraries(ANNA PRIVATE anna::rubberband)
endif()

if(ANNA_ENABLE_TRACKTION_ENGINE)
    target_compile_definitions(ANNA PRIVATE ANNA_HAS_TRACKTION_ENGINE=1)
    target_link_libraries(ANNA PRIVATE tracktion::tracktion_engine)
endif()

# -----------------------------------------------------------------------------
# Unit tests (console)
# -----------------------------------------------------------------------------
if(ANNA_BUILD_TESTS)
    juce_add_console_app(ANNA_Tests
        PRODUCT_NAME "ANNA Tests"
    )

    target_sources(ANNA_Tests
        PRIVATE
            Tests/SamprUnitTests.cpp
            Source/Assistant/TrackContextBuilder.cpp
            Source/Audio/LevelMeterBank.cpp
            Source/DSP/OnsetDetector.cpp
            Source/DSP/PeakGenerator.cpp
            Source/DSP/RubberBandOfflineProcessor.cpp
            Source/DSP/RubberBandStreamProcessor.cpp
            Source/Model/ProjectModel.cpp
            Source/Persistence/BeatSerializer.cpp
            Source/Persistence/ProjectSerializer.cpp
    )

    target_compile_definitions(ANNA_Tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=0
            $<$<BOOL:${ANNA_ENABLE_RUBBERBAND}>:ANNA_HAS_RUBBERBAND=1>
    )

    target_link_libraries(ANNA_Tests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_core
            juce::juce_data_structures
            juce::juce_graphics
    )

    if(ANNA_ENABLE_RUBBERBAND)
        target_link_libraries(ANNA_Tests PRIVATE anna::rubberband)
    endif()
endif()
