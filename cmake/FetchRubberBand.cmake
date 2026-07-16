# Fetches and builds Rubber Band Library as a static target.
# https://github.com/breakfastquay/rubberband

include(FetchContent)

FetchContent_Declare(
    rubberband
    GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git
    GIT_TAG        default
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(rubberband)

set(RUBBERBAND_SRC_DIR "${rubberband_SOURCE_DIR}")

set(RUBBERBAND_SOURCES
    ${RUBBERBAND_SRC_DIR}/src/rubberband-c.cpp
    ${RUBBERBAND_SRC_DIR}/src/RubberBandStretcher.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/AudioCurveCalculator.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/CompoundAudioCurve.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/HighFrequencyAudioCurve.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/SilentAudioCurve.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/PercussiveAudioCurve.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/StretcherChannelData.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/R2Stretcher.cpp
    ${RUBBERBAND_SRC_DIR}/src/faster/StretcherProcess.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/BQResampler.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/Profiler.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/Resampler.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/FFT.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/Log.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/Allocators.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/StretchCalculator.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/sysutils.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/mathmisc.cpp
    ${RUBBERBAND_SRC_DIR}/src/common/Thread.cpp
    ${RUBBERBAND_SRC_DIR}/src/finer/R3Stretcher.cpp
)

add_library(rubberband_static STATIC ${RUBBERBAND_SOURCES})

target_include_directories(rubberband_static
    PUBLIC
        ${RUBBERBAND_SRC_DIR}
        ${RUBBERBAND_SRC_DIR}/rubberband
        ${RUBBERBAND_SRC_DIR}/src
)

target_compile_definitions(rubberband_static
    PUBLIC
        RUBBERBAND_STATIC=1
    PRIVATE
        USE_BUILTIN_FFT=1
        USE_BQRESAMPLER=1
        NOMINMAX
        _USE_MATH_DEFINES
        $<$<CXX_COMPILER_ID:MSVC>:__MSVC__>
        $<$<CONFIG:Release>:NO_TIMING>
        $<$<CONFIG:Release>:NO_THREAD_CHECKS>
)

if(MSVC)
    target_compile_options(rubberband_static PRIVATE /W2)
else()
    target_compile_options(rubberband_static PRIVATE -Wall -Wno-unknown-pragmas)
endif()

add_library(anna::rubberband ALIAS rubberband_static)
message(STATUS "Rubber Band Library enabled from ${RUBBERBAND_SRC_DIR}")
