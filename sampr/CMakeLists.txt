cmake_minimum_required(VERSION 3.22)

project(SAMPR VERSION 0.9.0 LANGUAGES CXX)

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
# Disable with: cmake -B build -DSAMPR_ENABLE_RUBBERBAND=OFF
# -----------------------------------------------------------------------------
option(SAMPR_ENABLE_RUBBERBAND "Build with Rubber Band time/pitch DSP" ON)

if(SAMPR_ENABLE_RUBBERBAND)
    include(cmake/FetchRubberBand.cmake)
endif()

option(SAMPR_ENABLE_TRACKTION_ENGINE "Integrate Tracktion Engine module" OFF)
option(SAMPR_BUILD_TESTS "Build SAMPR unit test console app" ON)

if(SAMPR_ENABLE_TRACKTION_ENGINE)
    include(cmake/OptionalTracktionEngine.cmake)
endif()

# -----------------------------------------------------------------------------
# Standalone GUI application
# -----------------------------------------------------------------------------
juce_add_gui_app(SAMPR
    PRODUCT_NAME                "SAMPR"
    COMPANY_NAME                "SAMPR"
    BUNDLE_ID                   "com.sampr.app"
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT  "SAMPR needs microphone access for audio input."
)

juce_generate_juce_header(SAMPR)

# Single native desktop entry point: Source/Shell/Application.cpp (START_JUCE_APPLICATION)
target_sources(SAMPR
    PRIVATE
        Source/Shell/Application.cpp
        Source/Shell/MainComponent.cpp
        Source/App/MvpFlow.cpp
        Source/App/SampleImportService.cpp
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
        Source/Plugins/CompressorProcessor.cpp
        Source/Plugins/DelayProcessor.cpp
        Source/Plugins/ParametricEqProcessor.cpp
        Source/Plugins/ReverbProcessor.cpp
        Source/UI/ArrangementTimelineComponent.cpp
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

target_compile_definitions(SAMPR
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_MP3AUDIOFORMAT=1
        $<$<BOOL:${SAMPR_ENABLE_RUBBERBAND}>:SAMPR_HAS_RUBBERBAND=1>
)

target_link_libraries(SAMPR
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

if(SAMPR_ENABLE_RUBBERBAND)
    target_link_libraries(SAMPR PRIVATE sampr::rubberband)
endif()

if(SAMPR_ENABLE_TRACKTION_ENGINE)
    target_compile_definitions(SAMPR PRIVATE SAMPR_HAS_TRACKTION_ENGINE=1)
    target_link_libraries(SAMPR PRIVATE tracktion::tracktion_engine)
endif()

# -----------------------------------------------------------------------------
# Unit tests (console)
# -----------------------------------------------------------------------------
if(SAMPR_BUILD_TESTS)
    juce_add_console_app(SAMPR_Tests
        PRODUCT_NAME "SAMPR Tests"
    )

    target_sources(SAMPR_Tests
        PRIVATE
            Tests/SamprUnitTests.cpp
            Source/DSP/OnsetDetector.cpp
            Source/DSP/PeakGenerator.cpp
            Source/DSP/RubberBandOfflineProcessor.cpp
            Source/DSP/RubberBandStreamProcessor.cpp
            Source/Model/ProjectModel.cpp
            Source/Persistence/BeatSerializer.cpp
            Source/Persistence/ProjectSerializer.cpp
    )

    target_compile_definitions(SAMPR_Tests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=0
            $<$<BOOL:${SAMPR_ENABLE_RUBBERBAND}>:SAMPR_HAS_RUBBERBAND=1>
    )

    target_link_libraries(SAMPR_Tests
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_core
            juce::juce_data_structures
            juce::juce_graphics
    )

    if(SAMPR_ENABLE_RUBBERBAND)
        target_link_libraries(SAMPR_Tests PRIVATE sampr::rubberband)
    endif()
endif()