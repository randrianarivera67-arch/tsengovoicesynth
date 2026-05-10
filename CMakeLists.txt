cmake_minimum_required(VERSION 3.15)
project(TsengoVoiceSynth VERSION 1.0.0)

# JUCE submodule
add_subdirectory(JUCE)

juce_add_plugin(TsengoVoiceSynth
    COMPANY_NAME "Tsengo"
    PLUGIN_MANUFACTURER_CODE Tsgn
    PLUGIN_CODE Tvs1
    FORMATS VST3
    PRODUCT_NAME "Tsengo Voice Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PLUGIN_MIDI_INPUT TRUE
    NEEDS_AUDIO_INPUT TRUE
)

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
)
