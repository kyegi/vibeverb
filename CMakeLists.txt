cmake_minimum_required(VERSION 3.22)
project(JuceReverb VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(JuceReverb
    COMPANY_NAME             "MyStudio"
    PLUGIN_MANUFACTURER_CODE MySt
    PLUGIN_CODE              Rvb1
    FORMATS                  VST3
    PRODUCT_NAME             "JuceReverb"
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST       FALSE)

target_sources(JuceReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(JuceReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(JuceReverb PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if (WIN32)
    foreach(_fmt VST3 Standalone)
        if (TARGET JuceReverb_${_fmt})
            target_link_options(JuceReverb_${_fmt} PRIVATE
                -static-libgcc -static-libstdc++ -static
                -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive)
        endif()
    endforeach()
endif()
