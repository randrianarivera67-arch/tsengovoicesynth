cmake_minimum_required(VERSION 3.22)
project(TsengoVoiceSynth VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch JUCE ───────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── Plugin target ─────────────────────────────────────────────────────────────
# IS_SYNTH TRUE  → appears in FL Studio Channel Rack (not the Mixer)
# NEEDS_MIDI_OUTPUT TRUE → sends notes directly to the piano roll
juce_add_plugin(TsengoVoiceSynth
    COMPANY_NAME                "Tsengo"
    PLUGIN_MANUFACTURER_CODE    "Tsnk"
    PLUGIN_CODE                 "Tvs2"
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Tsengo Voice Synth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             "Instrument" "Tools"
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

# ── CRITICAL: Generate JuceHeader.h ──────────────────────────────────────────
juce_generate_juce_header(TsengoVoiceSynth)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(TsengoVoiceSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/YinPitchDetector.cpp
    Source/CrystalLookAndFeel.cpp
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(TsengoVoiceSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# ── JUCE modules ──────────────────────────────────────────────────────────────
target_link_libraries(TsengoVoiceSynth PRIVATE
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
)
