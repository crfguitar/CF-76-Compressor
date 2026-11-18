cmake_minimum_required(VERSION 3.22)
project(CF76 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "C:/JUCE" CACHE PATH "Path to JUCE source")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

juce_add_plugin(CF76
    COMPANY_NAME "CF"
    PLUGIN_MANUFACTURER_CODE Cfxx
    PLUGIN_CODE Cf76
    FORMATS VST3 Standalone
    PRODUCT_NAME "CF-76"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Program Files/Common Files/VST3"
    VST3_CATEGORIES Fx Dynamics)

target_sources(CF76 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/DevTests.cpp
    Source/DSP/FetCompressor.cpp
    Source/UI/Skin.cpp
    Source/UI/Knob.cpp
    Source/UI/Buttons.cpp
    Source/UI/VUMeter.cpp
    Source/UI/Segmented.cpp
    Source/UI/TopBar.cpp)

target_compile_definitions(CF76 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CF76
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: loads the built VST3 like a DAW (create / editor / delete / unload) to catch teardown crashes.
juce_add_console_app(CF76HostTest PRODUCT_NAME "CF76HostTest")
target_sources(CF76HostTest PRIVATE tools/hosttest/HostTest.cpp)
target_compile_definitions(CF76HostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CF76HostTest PRIVATE juce::juce_audio_processors juce::juce_gui_basics
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
