cmake_minimum_required(VERSION 3.21)
project(TsengoVoiceSynth VERSION 5.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The DSP core builds without JUCE, so CI can run the unit tests in seconds
# instead of waiting for the framework to be fetched and compiled.
option(TSENGO_TESTS_ONLY "Build only the framework-free unit tests" OFF)

if(TSENGO_TESTS_ONLY)
    enable_testing()
    add_executable(TsengoTests Tests/test_pitch_tracker.cpp Source/PitchTracker.cpp)
    target_include_directories(TsengoTests PRIVATE Source)
    add_test(NAME dsp COMMAND TsengoTests)
    return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(TSENGO_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/FormantDetector.cpp
    Source/PitchTracker.cpp
    Source/TriggerEngine.cpp)

# ---------------------------------------------------------------------------
# One code base, three products:
#   * Synth VST3      — FL Studio Channel Rack (opens its own mic device)
#   * Effect VST3     — insert on the mic track, audio in -> MIDI out
#   * Standalone app  — mic in, MIDI out to loopMIDI / IAC (like Dubler's app)
# ---------------------------------------------------------------------------
function(tsengo_add_target target productName pluginCode formats isSynth)
    juce_add_plugin(${target}
        COMPANY_NAME                "Tsengo"
        PLUGIN_MANUFACTURER_CODE    Tsgn
        PLUGIN_CODE                 ${pluginCode}
        FORMATS                     ${formats}
        PRODUCT_NAME                ${productName}
        IS_SYNTH                    ${isSynth}
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           TRUE
        IS_MIDI_EFFECT              FALSE
        NEEDS_AUDIO_INPUT           TRUE
        NEEDS_AUDIO_OUTPUT          TRUE)

    juce_generate_juce_header(${target})
    target_sources(${target} PRIVATE ${TSENGO_SOURCES})

    target_compile_definitions(${target} PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1)

    target_link_libraries(${target} PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_gui_basics
        juce::juce_dsp)
endfunction()

tsengo_add_target(TsengoVoiceSynth "Tsengo Voice Synth"    Tvs5 "VST3"       TRUE)
tsengo_add_target(TsengoVoiceFX    "Tsengo Voice FX"       Tvf5 "VST3"       FALSE)
tsengo_add_target(TsengoVoiceApp   "Tsengo Voice"          Tva5 "Standalone" TRUE)
