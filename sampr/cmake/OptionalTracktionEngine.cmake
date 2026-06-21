# Optional Tracktion Engine integration
#
# Prerequisites:
#   git submodule add https://github.com/Tracktion/tracktion_engine.git external/tracktion_engine
#   cd external/tracktion_engine && git submodule update --init --recursive
#
# Tracktion Engine requires C++20 and a separate commercial/GPL license.
# See: https://github.com/Tracktion/tracktion_engine

set(TE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../external/tracktion_engine" CACHE PATH "Tracktion Engine root")

if(NOT EXISTS "${TE_ROOT}/modules/tracktion_engine/tracktion_engine.h")
    message(FATAL_ERROR
        "Tracktion Engine not found at ${TE_ROOT}.\n"
        "Clone it: git clone --recurse-submodules https://github.com/Tracktion/tracktion_engine.git external/tracktion_engine")
endif()

# JUCE must be available before adding the TE module
add_subdirectory("${TE_ROOT}/modules" "${CMAKE_BINARY_DIR}/tracktion_modules")

add_library(tracktion::tracktion_engine ALIAS tracktion_engine)

message(STATUS "Tracktion Engine enabled from ${TE_ROOT}")