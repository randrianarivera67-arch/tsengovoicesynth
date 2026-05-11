#pragma once
#include <JuceHeader.h>
#include "YinPitchDetector.h"

/**
 * TsengoVoiceSynth v2.1
 *
 * FL Studio Channel Rack instrument.
 * Mic audio arrives via sidechain bus from FL Studio Mixer.
 * Outputs MIDI noteOn/noteOff to the piano roll.
 */
class TsengoVoiceSynthProcessor : public juce::AudioProcessor
{
public:
    TsengoVoiceSynthProcessor();
    ~TsengoVoiceSynthProcessor() override;

    void prepareToPlay   (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock    (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool   hasEditor()      const override { return true; }
    juce::String getName()  const override { return "Tsengo Voice Synth"; }
    bool   acceptsMidi()    const override { return true; }
    bool   producesMidi()   const override { return true; }
    bool   isMidiEffect()   const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── Shared state (audio thread → UI thread) ───────────────────────────
    struct DetectionState
    {
        float frequency    { 0.0f };
        float confidence   { 0.0f };
        int   midiNote     { -1   };
        float midiCents    { 0.0f };
        float inputLevel   { 0.0f };
        bool  micConnected { false };
    };
    DetectionState getDetectionState() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    std::atomic<float>* pThreshold { nullptr };
    std::atomic<float>* pVolume    { nullptr };
    std::atomic<float>* pAttack    { nullptr };
    std::atomic<float>* pRelease   { nullptr };

    // 4096-sample buffer = ~93ms at 44.1kHz — better low-freq resolution
    static constexpr int kYinBuf { 4096 };
    YinPitchDetector yin_;

    // MIDI state machine
    int currentNote_    { -1 };
    int candidateNote_  { -1 };
    int candidateCount_ { 0  };
    int releaseCounter_ { 0  };

    mutable juce::SpinLock stateLock_;
    DetectionState sharedState_;

    void emitNoteOn  (int note, uint8_t velocity, juce::MidiBuffer&, int offset);
    void emitNoteOff (int note, juce::MidiBuffer&, int offset);
    int  attackSamples()  const noexcept;
    int  releaseSamples() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoVoiceSynthProcessor)
};
