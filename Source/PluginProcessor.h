#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <vector>
#include <map>
#include <memory>
#include "FormantDetector.h"
#include "PitchTracker.h"
#include "TriggerEngine.h"

class TsengoProcessor : public juce::AudioProcessor,
                        private juce::AudioIODeviceCallback
{
public:
    TsengoProcessor();
    ~TsengoProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return name_; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return true;  }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return preset_; }
    void changeProgramName (int, const juce::String&) override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Where the voice comes from.
    //   Auto — host audio if the DAW/standalone wrapper feeds us any, otherwise
    //          the plugin's own microphone device.
    //   Host — always the plugin's audio input bus (effect / standalone build).
    //   Mic  — always the internal device (needed in FL Studio's synth slot).
    enum InputSource { SourceAuto = 0, SourceHost, SourceMic };
    std::atomic<int> inputSource { SourceAuto };

    bool isUsingHostInput() const { return usingHost_.load(); }
    float getHostInputLevel() const { return hostLevel_.load(); }

    //==========================================================================
    // External MIDI output — how the standalone build reaches a DAW
    // (loopMIDI / IAC bus), exactly like Dubler's app does.
    juce::StringArray getMidiOutputs();
    void openMidiOutput (const juce::String& name);
    void closeMidiOutput();
    juce::String getCurrentMidiOutput() const { return midiOutName_; }
    bool isMidiOutputOpen() const { return midiOutOpen_.load(); }

    //==========================================================================
    // Mic device
    juce::StringArray getInputDevices();
    void openDevice (const juce::String& name);
    void closeDevice();
    juce::String getCurrentDevice() const { return currentDevice_; }
    bool isDeviceOpen() const { return deviceOpen_; }

    // UI read-outs
    float getMicLevel()   const { return micLevel_.load();   }
    float getMidiLevel()  const { return midiLevel_.load();  }
    int   getNote()       const { return currentNote_.load();}
    float getPitchHz()    const { return currentHz_.load();  }
    float getConfidence() const { return confidence_.load(); }

    // Vowel / formant read-outs — aaa/eee/ooo mirror Dubler 2's vowel pads
    float getFormant1() const { return f1_.load(); }
    float getFormant2() const { return f2_.load(); }
    float getVowelAAA() const { return vowelAAA_.load(); }
    float getVowelEEE() const { return vowelEEE_.load(); }
    float getVowelOOO() const { return vowelOOO_.load(); }
    float getVowelEnv() const { return vowelEnv_.load(); }

    //==========================================================================
    // Trainable trigger pads (see TriggerEngine.h)
    TriggerEngine& triggers() { return trig_; }
    static juce::String defaultPadName (int pad);
    std::atomic<int> padNote[TriggerEngine::kNumPads];

    //==========================================================================
    // Calibration wizard — a guided mic setup like Dubler's, but it works with
    // whatever microphone you own instead of requiring their USB mic.
    enum CalStage
    {
        CalIdle = 0,
        CalAmbient,     // stay silent   -> noise floor + threshold
        CalLevel,       // sing normally -> input gain
        CalLowNote,     // lowest note   -> pitch range floor
        CalHighNote,    // highest note  -> pitch range ceiling
        CalDone
    };

    void startCalibration();
    void cancelCalibration();
    int   getCalStage()    const { return calStage_.load(); }
    float getCalProgress() const { return calProgress_.load(); }
    juce::String getCalInstruction() const;

    float getPitchMinHz() const { return pitchMinHz_.load(); }
    float getPitchMaxHz() const { return pitchMaxHz_.load(); }
    void  setPitchRange (float lo, float hi);
    float getNoiseFloor() const { return noiseFloor_.load(); }

    //==========================================================================
    // Parameters — mic capture
    std::atomic<float> threshold { 0.10f };
    std::atomic<float> smoothing { 0.15f };
    std::atomic<float> gain      { 1.00f };

    // Analysis window: 0 = 1024 (fast), 1 = 2048 (balanced), 2 = 4096 (accurate)
    std::atomic<int> latencyMode { 1 };

    // Key / Scale quantize. scaleType: 0=Chromatic 1=Major 2=Minor
    // 3=MajorPentatonic 4=MinorPentatonic 5=Dorian
    std::atomic<int>  keyRoot       { 0 };     // 0=C .. 11=B
    std::atomic<int>  scaleType     { 1 };
    std::atomic<bool> quantizeToKey { false };

    // Chords. chordType: 0=Major 1=Minor 2=Sus4 3=Maj7 4=Min7 5=Octave
    std::atomic<bool> chordsEnabled { false };
    std::atomic<int>  chordType     { 0 };

    // Pitch bend — carries fine vocal pitch/vibrato under the quantized note
    std::atomic<bool> pitchBendEnabled       { true };
    std::atomic<int>  pitchBendRangeSemitones{ 2 };

    // Time quantize — snaps note onsets to the host tempo grid.
    // division: subdivisions per beat (1=1/4, 2=1/8, 4=1/16, 8=1/32)
    std::atomic<bool> timeQuantizeEnabled { false };
    std::atomic<int>  timeQuantizeDivision{ 4 };

    // Monitor synth — simple built-in tone so you can hear pitch while singing
    std::atomic<bool>  monitorSynthEnabled { false };
    std::atomic<float> monitorVolume       { 0.3f };

    // Octave shift applied to the note sent to the DAW (-2..+2 octaves)
    std::atomic<int> octaveShift { 0 };

    // Stickiness — how many analysis frames a note must hold before
    // retriggering (higher = less flicker, slower response)
    std::atomic<int> stickiness { 3 };

    // Assign — MIDI CC numbers used for the four vowel/envelope macros
    std::atomic<int> ccAaa { 20 };
    std::atomic<int> ccEee { 21 };
    std::atomic<int> ccOoo { 22 };
    std::atomic<int> ccEnv { 23 };

    // Assign — MIDI output channels (1..16)
    std::atomic<int> pitchChannel   { 1  };
    std::atomic<int> triggerChannel { 10 };

    // Editor sync — bumped when the host restores a saved state
    std::atomic<int> stateRevision { 0 };

private:
    //==========================================================================
    // AudioIODeviceCallback — mic capture
    void audioDeviceIOCallbackWithContext(
        const float* const* in, int numIn,
        float* const* out,     int numOut,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    //==========================================================================
    // Key/Scale quantize + chords helpers
    int  quantizeNoteToScale (int note) const;
    void getChordIntervals   (int type, std::vector<int>& intervals) const;

    // Note event dispatch (handles chord notes + pitch-bend reset together)
    void sendNoteChange (juce::MidiBuffer& midi, int newNote, int velocity, int sampleOffset);
    void allNotesOff    (juce::MidiBuffer& midi);

    // Time quantize — finds the next tempo-grid sample offset within this block
    bool nextGridOffsetInBlock (int blockSize, int& sampleOffset);

    // Trigger pads — drains the engine's FIFO and emits MIDI
    void serviceTriggers (juce::MidiBuffer& midi, int blockSize);

    // Pushes host audio into the same ring buffer the mic callback fills
    void captureHostInput (const juce::AudioBuffer<float>& buffer);

    // Mirrors everything we generated to the external MIDI port, if open
    void forwardToMidiOutput (const juce::MidiBuffer& midi, int blockSize);

    // Calibration wizard state machine (runs once per processBlock)
    void serviceCalibration (float level, float hz, float conf, int blockSize);
    void advanceCalStage();

    //==========================================================================
    // Mic ring buffer
    static constexpr int RING    = 65536;
    static constexpr int MAX_WIN = 4096;
    std::array<float, RING> ring_ {};
    std::atomic<int>  ringWrite_ { 0 };
    float             yinBuf_[MAX_WIN] {};
    PitchTracker      pitch_;

    // MIDI state
    std::atomic<int>   currentNote_ { -1 };
    std::atomic<float> currentHz_   { 0.f };
    std::atomic<float> confidence_  { 0.f };
    std::atomic<float> micLevel_    { 0.f };
    std::atomic<float> midiLevel_   { 0.f };

    // Vowel / formant state
    FormantDetector    formant_;
    std::atomic<float> f1_       { 0.f };
    std::atomic<float> f2_       { 0.f };
    std::atomic<float> vowelAAA_ { 0.f };
    std::atomic<float> vowelEEE_ { 0.f };
    std::atomic<float> vowelOOO_ { 0.f };
    std::atomic<float> vowelEnv_ { 0.f };
    // Audio-thread-only smoothing state (not shared, no atomics needed)
    float smAaa_ { 0.f }, smEee_ { 0.f }, smOoo_ { 0.f }, smEnv_ { 0.f };
    int   lastCcAaa_ { -1 }, lastCcEee_ { -1 }, lastCcOoo_ { -1 }, lastCcEnv_ { -1 };

    int  lastMidiNote_ { -1 };
    int  pendingNote_  { -2 };   // -2 = nothing pending, -1 = note-off pending
    int  noteHoldCount_{ 0  };

    std::vector<int>      activeExtraNotes_;   // chord notes beyond the root
    std::map<int, double>  voicePhase_;        // monitor synth oscillator phases
    int lastBendValue_ { 8192 };

    //==========================================================================
    // Trigger engine + note lifetimes
    TriggerEngine trig_;
    struct ActiveTrig { int note = -1; int samplesLeft = 0; };
    std::array<ActiveTrig, TriggerEngine::kNumPads> trigActive_ {};

    //==========================================================================
    // Calibration
    std::atomic<int>   calStage_    { CalIdle };
    std::atomic<float> calProgress_ { 0.f };
    std::atomic<float> noiseFloor_  { 0.01f };
    std::atomic<float> pitchMinHz_  { 70.f  };
    std::atomic<float> pitchMaxHz_  { 1100.f };
    double calElapsed_ { 0.0 };
    double calTarget_  { 2.0 };
    float  calAccum_   { 0.f };
    int    calCount_   { 0 };
    float  calPeak_    { 0.f };
    std::vector<float> calPitches_;

    // Input routing
    std::atomic<bool>  usingHost_ { false };
    std::atomic<float> hostLevel_ { 0.f };
    std::vector<float> hostMono_;

    // External MIDI output
    std::unique_ptr<juce::MidiOutput> midiOut_;
    juce::SpinLock   midiOutLock_;
    juce::String     midiOutName_;
    std::atomic<bool> midiOutOpen_ { false };
    double midiOutClock_ { 0.0 };

    // Device manager
    juce::AudioDeviceManager micMgr_;
    juce::String currentDevice_;
    bool deviceOpen_ { false };
    double sampleRate_ { 44100.0 };

    const juce::String name_   { "Tsengo Voice Synth" };
    const juce::String preset_ { "Default" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoProcessor)
};
