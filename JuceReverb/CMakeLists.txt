cmake_minimum_required(VERSION 3.22)
project(JuceReverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# JUCE — clone alongside this project: git clone https://github.com/juce-framework/JUCE.git
# -----------------------------------------------------------------------
add_subdirectory(JUCE)

juce_add_plugin(JuceReverb
    COMPANY_NAME            "MyStudio"
    PLUGIN_MANUFACTURER_CODE MySt
    PLUGIN_CODE             Rvb1
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "JuceReverb"
)

juce_generate_juce_header(JuceReverb)

target_sources(JuceReverb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(JuceReverb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(JuceReverb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
