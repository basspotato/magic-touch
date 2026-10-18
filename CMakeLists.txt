cmake_minimum_required(VERSION 3.22)
project(MagicTouch VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Don't try to copy into system plugin folders on CI machines
if(DEFINED ENV{CI})
    set(MT_COPY FALSE)
else()
    set(MT_COPY TRUE)
endif()

juce_add_plugin(MagicTouch
    COMPANY_NAME "YourName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Mgtc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Magic Touch"
    COPY_PLUGIN_AFTER_BUILD ${MT_COPY})

target_sources(MagicTouch PRIVATE
    Source/Params.h
    Source/LookAndFeel.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/SpectrumAnalyzer.h
    Source/SpectrumAnalyzer.cpp)

target_compile_definitions(MagicTouch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(MagicTouch
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
