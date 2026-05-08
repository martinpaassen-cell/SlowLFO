cmake_minimum_required(VERSION 3.22)
project(SlowLFO VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)
juce_add_plugin(SlowLFO
    VERSION 1.0.0 COMPANY_NAME "Liven"
    PLUGIN_MANUFACTURER_CODE Lvnb PLUGIN_CODE Slfo
    FORMATS VST3 Standalone PRODUCT_NAME "Slow LFO"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
juce_generate_juce_header(SlowLFO)
target_sources(SlowLFO PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(SlowLFO PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(SlowLFO
    PRIVATE
        juce::juce_audio_basics juce::juce_audio_devices juce::juce_audio_formats
        juce::juce_audio_plugin_client juce::juce_audio_processors juce::juce_audio_utils
        juce::juce_core juce::juce_data_structures juce::juce_events
        juce::juce_graphics juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
