#pragma once
#include <JuceHeader.h>

enum class SourceMode { SYNTH, MIC };
enum class OscType    { SINE, SAW, SQUARE, TRIANGLE };

class VoiceSynthProcessor : public juce::AudioProcessor,
                             private juce::AudioIODeviceCallback
{
public:
    VoiceSynthProcessor();
    ~VoiceSynthProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tsengo Voice Synth"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // === State public (editor manavao azy) ===
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<int>   currentMidiNote { -1 };

    SourceMode sourceMode { SourceMode::SYNTH };
    OscType    oscType    { OscType::SINE };
    float      volume     { 0.8f };
    float      attack     { 0.02f };
    float      release    { 0.4f };

    // === Mic device management ===
    juce::StringArray getAvailableInputDevices();
    void openMicDevice(const juce::String& deviceName);
    juce::AudioDeviceManager& getMicDeviceManager() { return micDeviceManager; }

private:
    void audioDeviceIOCallbackWithContext(
        const float* const* in, int numIn,
        float* const* out,     int numOut,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    juce::AudioDeviceManager micDeviceManager;
    bool micInitialized = false;

    static constexpr int MIC_BUF = 176400;
    juce::AudioBuffer<float> micBuffer;
    std::atomic<int> micWritePos { 0 };

    // Oscillator
    double oscPhase    = 0.0;
    double sampleRate_ = 44100.0;

    // ADSR
    enum class AState { IDLE, ATTACK, SUSTAIN, RELEASE };
    AState adsrState = AState::IDLE;
    float  adsrLevel = 0.0f;

    // MIDI / pitch
    float pitchRatio = 1.0f;
    static constexpr int BASE_NOTE = 60;

    static float midiToFreq(int n)
    { return 440.0f * std::pow(2.0f, (n - 69) / 12.0f); }

    float nextOscSample();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSynthProcessor)
};
