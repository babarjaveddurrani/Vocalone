cmake_minimum_required(VERSION 3.22)
project(VocalOne VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(VocalOne
    COMPANY_NAME "Durrani Studio"
    BUNDLE_ID com.durranistudio.vocalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Drni
    PLUGIN_CODE Vo1c
    VST3_CATEGORIES Fx Dynamics EQ
    FORMATS VST3 Standalone
    PRODUCT_NAME "VocalOne")

target_sources(VocalOne PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VocalOne PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VocalOne
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---- headless DSP tests ----
enable_testing()
add_executable(dsp_tests Tests/dsp_tests.cpp)
target_link_libraries(dsp_tests PRIVATE juce::juce_dsp juce::juce_audio_basics)
target_compile_definitions(dsp_tests PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
add_test(NAME dsp_tests COMMAND dsp_tests)

# ---- optional: headless screenshot of the interface (developer tool) ----
option(VOCALONE_UI_SNAPSHOT "Build the ui_snapshot tool" OFF)
if(VOCALONE_UI_SNAPSHOT)
    add_executable(ui_snapshot Tests/ui_snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_link_libraries(ui_snapshot PRIVATE juce::juce_audio_utils juce::juce_dsp)
    target_compile_definitions(ui_snapshot PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
