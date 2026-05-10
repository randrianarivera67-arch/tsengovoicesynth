#pragma once
#include <JuceHeader.h>

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    float getInputLevel() const  { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    int   getCurrentNote() const { return currentMidiNote.load(); }

private:
    // === Mic capture callbacks ===
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData, int numInputChannels,
        float* const* outputChannelData,      int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    // Internal mic device manager (WASAPI — tsy mifangaro amin'ny ASIO FL Studio)
    juce::AudioDeviceManager micDeviceManager;
    bool micInitialized = false;

    // Ring buffer
    static constexpr int MIC_BUFFER_SIZE = 88200;
    juce::AudioBuffer<float> micBuffer;
    std::atomic<int> micWritePos { 0 };

    // MIDI
    std::atomic<int> currentMidiNote { -1 };
    float currentPitchRatio = 1.0f;
    static constexpr int BASE_NOTE = 60;

    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    static float midiNoteToFreq(int note)
    {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSynthProcessor)
};
