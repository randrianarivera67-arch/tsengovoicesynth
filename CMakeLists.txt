cmake_minimum_required(VERSION 3.21)
project(TsengoVoiceSynth VERSION 3.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TsengoVoiceSynth
    COMPANY_NAME                "Tsengo"
    PLUGIN_MANUFACTURER_CODE    Tsgn
    PLUGIN_CODE                 Tvs3
    FORMATS                     VST3
    PRODUCT_NAME                "Tsengo Voice Synth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    NEEDS_AUDIO_INPUT           FALSE
    NEEDS_AUDIO_OUTPUT          TRUE
)

juce_generate_juce_header(TsengoVoiceSynth)

target_sources(TsengoVoiceSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(TsengoVoiceSynth PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(TsengoVoiceSynth PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp
)
