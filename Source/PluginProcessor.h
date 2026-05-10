#pragma once
#include <JuceHeader.h>

class VoiceSynthProcessor : public juce::AudioProcessor
{
public:
    VoiceSynthProcessor();
    ~VoiceSynthProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Tsengo Voice Synth"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // Parameters azo jerena avy amin'ny Editor
    float getInputLevel() const  { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }
    int   getCurrentNote() const { return currentMidiNote.load(); }

private:
    // Buffer hitahiry feo microphone (ring buffer)
    static constexpr int MIC_BUFFER_SIZE = 88200; // 2s @ 44100Hz
    juce::AudioBuffer<float> micBuffer;
    std::atomic<int> micWritePos { 0 };

    // MIDI state
    std::atomic<int> currentMidiNote { -1 };
    float currentPitchRatio = 1.0f;

    // Nota fototra = Middle C (60) = feo normal tsy manova pitch
    static constexpr int BASE_NOTE = 60;

    // Level meters ho an'ny UI
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    // Helper: MIDI note → frequency
    static float midiNoteToFreq(int note)
    {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoiceSynthProcessor)
};
