cmake_minimum_required(VERSION 3.22)

# Set deployment target before project (macOS only)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")
endif()

project(EnzoGain VERSION 1.0.0)

# --- Locate JUCE ---
# Priority: 1) JUCE_PATH variable  2) local deps  3) auto-download via FetchContent
if(NOT DEFINED JUCE_PATH)
    # Check common local locations
    if(EXISTS "$ENV{HOME}/plugin-freedom-system/deps/JUCE/CMakeLists.txt")
        set(JUCE_PATH "$ENV{HOME}/plugin-freedom-system/deps/JUCE")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    endif()
endif()

if(DEFINED JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using local JUCE from: ${JUCE_PATH}")
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    message(STATUS "JUCE not found locally — downloading via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin formats: platform-aware
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
elseif(WIN32)
    set(PLUGIN_FORMATS VST3 Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(EnzoGain
    COMPANY_NAME "EnzoShah"
    PLUGIN_MANUFACTURER_CODE Enzo
    PLUGIN_CODE EGan
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "EnzoGain"
    NEEDS_WEB_BROWSER TRUE
)

# WebView UI Resources - embed all UI files into plugin binary
juce_add_binary_data(EnzoGain_UIResources
    SOURCES
        Source/ui/public/index.html
        Source/ui/public/js/juce/index.js
        Source/ui/public/js/juce/check_native_interop.js
        Source/ui/public/logo.png
        Source/ui/public/spicy.png
)

# Source files
target_sources(EnzoGain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# Include paths
target_include_directories(EnzoGain
    PRIVATE
        Source
)

# Required JUCE modules
target_link_libraries(EnzoGain
    PRIVATE
        EnzoGain_UIResources
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
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

# Compile definitions
target_compile_definitions(EnzoGain
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)
